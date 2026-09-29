#!/usr/bin/env python3
"""
HTTP Server Service Module

Contains the core web server functionality for WiSUN Applications.
"""

import json
import logging
import os
import secrets
import signal
import socket
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

from flask import Flask, jsonify, request
from service.websocket import WebSocketService

from openapi_core import OpenAPI
from openapi_core.contrib.flask.decorators import FlaskOpenAPIViewDecorator

import service.executor as executor

# Configuration
DEFAULT_HOST = '127.0.0.1'
DEFAULT_PORT = 9080

# Paths that do not require token authentication
TOKEN_EXCLUDED_PATHS = {'/', '/api/info'}

openapi = OpenAPI.from_file_path(Path(__file__).parent / 'ddp-openapi.json')
openapi_validated = FlaskOpenAPIViewDecorator(openapi)

# Logger names used by executor and wisunpki (do not modify those modules)
_EXECUTION_LOGGER_NAMES = ('executor', 'wisunpki')


class WebSocketLogHandler(logging.Handler):
    """Captures log records and sends them to a callback (e.g. WebSocket) without modifying executor/wisunpki."""

    def __init__(self, send_callback):
        super().__init__()
        self._send_callback = send_callback

    def emit(self, record):
        try:
            msg = self.format(record)
            level = record.levelname.lower() if record.levelname else 'info'
            self._send_callback(msg, level)
        except Exception:
            self.handleError(record)


class HttpServer:
    """HTTP Server management class"""

    def __init__(self, host=None, port=None, pid_file=None, log_file=None, timer_file=None, token_file=None, cwd=None):
        self.host = host or DEFAULT_HOST
        self.port = port or DEFAULT_PORT
        self.cwd = cwd or os.getcwd()
        self.pid_file = pid_file or Path(self.cwd) / 'server.pid'
        self.log_file = log_file or Path(self.cwd) / 'server.log'
        self.timer_file = timer_file or Path(self.cwd) / 'internal_timer.timestamp'
        self.token_file = token_file or Path(self.cwd) / 'auth.token'
        self.internal_timer = None
        self.watchdog_thread = None
        self.shutdown_flag = False

        # Create Flask app
        self.app = Flask(__name__)

        # Initialize WebSocket service first (without common_executor for now)
        self.websocket_service = WebSocketService(self.app, None, lambda: self.shutdown_flag)
        self.socketio = self.websocket_service.socketio

        # Initialize common executor and pass the websocket service to it
        #self.common_executor = CommonExecutor(ExecutionMode.WEB_SERVICE, self.websocket_service)

        # Now set the common_executor in the websocket_service
        #self.websocket_service.common_executor = self.common_executor

        # Configure CORS
        self._setup_cors()

        # Initialize token for authentication
        self._token = self._get_or_create_token()

        self._setup_routes()

    def _stream_execution_logs(self, run_fn, *args, **kwargs):
        """Run run_fn(*args, **kwargs) while streaming all logs from provision/wisunpki to WebSocket."""
        handler = WebSocketLogHandler(
            lambda msg, level: self.websocket_service.send_message(
                'message', {'message': msg, 'level': level})
        )
        handler.setFormatter(logging.Formatter('%(message)s'))
        loggers = [logging.getLogger(name) for name in _EXECUTION_LOGGER_NAMES]
        for log in loggers:
            log.addHandler(handler)
        try:
            return run_fn(*args, **kwargs)
        finally:
            for log in loggers:
                log.removeHandler(handler)

    def _setup_cors(self):
        """Setup Cross-Origin Resource Sharing (CORS) headers"""
        @self.app.after_request
        def after_request(response):
            # Allow requests from localhost on any port for development
            origin = request.headers.get('Origin')
            if origin and ('localhost' in origin or '127.0.0.1' in origin):
                response.headers['Access-Control-Allow-Origin'] = origin
            else:
                # For production, you might want to be more restrictive
                response.headers['Access-Control-Allow-Origin'] = '*'

            response.headers['Access-Control-Allow-Methods'] = 'GET, POST, PUT, DELETE, OPTIONS'
            response.headers['Access-Control-Allow-Headers'] = 'Content-Type, Authorization'
            response.headers['Access-Control-Allow-Credentials'] = 'true'
            return response

        @self.app.route('/<path:path>', methods=['OPTIONS'])
        @self.app.route('/', methods=['OPTIONS'])
        def handle_options(path=None):
            """Handle preflight OPTIONS requests"""
            return '', 200


    def _get_or_create_token(self):
        """Get token from file or create a new one and persist to file."""
        try:
            if self.token_file.exists():
                with open(self.token_file, 'r') as f:
                    token = f.read().strip()
                    if token:
                        return token
        except (OSError, IOError):
            pass

        token = secrets.token_urlsafe(32)
        try:
            with open(self.token_file, 'w') as f:
                f.write(token)
        except OSError as e:
            print(f"Warning: Could not write token file: {e}")
        return token

    def _validate_request_token(self):
        """Validate token from request. Returns None if valid, or error response if invalid."""
        if request.method == 'OPTIONS':
            return None

        auth_header = request.headers.get('Authorization')
        if auth_header and auth_header.startswith('Bearer '):
            token = auth_header[7:]
        else:
            token = request.args.get('token')

        if token and secrets.compare_digest(token, self._token):
            return None
        return jsonify({'error': 'Unauthorized', 'message': 'Valid token required'}), 401

    def _setup_routes(self):
        """Setup Flask routes"""
        import traceback
        from werkzeug.exceptions import HTTPException

        @self.app.errorhandler(Exception)
        def handle_exception(err):
            """Log internal server errors and traceback to WebSocket so UI shows them."""
            if isinstance(err, HTTPException):
                raise err
            try:
                tb = traceback.format_exc()
                self._log(f"Internal server error: {err}", level='error')
                for line in tb.strip().splitlines():
                    self._log(line, level='error')
            except Exception:
                pass
            return jsonify({'error': 'Internal server error', 'message': str(err)}), 500

        @self.app.before_request
        def before_api():
            if request.path not in TOKEN_EXCLUDED_PATHS:
                auth_error = self._validate_request_token()
                if auth_error is not None:
                    return auth_error
            # Keep the server alive based on API activity
            self.set_internal_timer()
            return None

        @self.app.route('/')
        def home():
            """Home endpoint"""
            return jsonify({
                'status': 'running',
                'message': 'WiSUN Applications Server is running',
                'pid': os.getpid()
            })

        @self.app.route('/api/info')
        def info():
            """Server information endpoint"""
            return jsonify({
                'server': 'WiSUN Applications Server',
                'pid': os.getpid(),
                'host': request.host,
                'uptime': time.time() - getattr(self.app, 'start_time', time.time()),
                'websocket_url': f'ws://{request.host}/socket.io/?transport=websocket',
                'plain_websocket_available': False, # We don't have plain WebSocket yet
                'token_file': str(self.token_file.resolve()),
                'boards': executor.get_ddp_ram_board_ids()
            })

        @self.app.route('/resetTimer', methods=['GET'])
        def reset_timer():
            """Reset the internal timer to prevent server shutdown"""
            old_timer = self.get_internal_timer()
            new_timer = self.set_internal_timer()
            print(f"[DEBUG] Worker {os.getpid()}: Timer manually reset from {old_timer} to {new_timer}")
            return jsonify({
                'status': 'timer_reset',
                'previous_timer': old_timer,
                'new_timer': new_timer,
                # 5 minutes = 300 seconds
                'time_remaining': 300 - (time.time() - new_timer) if new_timer else 0
            })

        @self.app.route('/timerStatus', methods=['GET'])
        def timer_status():
            """Get the current timer status"""
            timer_value = self.get_internal_timer()
            if timer_value:
                current_time = time.time()
                elapsed_time = current_time - timer_value
                # 5 minutes = 300 seconds
                remaining_time = max(0, 300 - elapsed_time)

                return jsonify({
                    'timer_initialized': True,
                    'worker_pid': os.getpid(),
                    'current_time': current_time,
                    'timer_start': timer_value,
                    'elapsed_seconds': elapsed_time,
                    'remaining_seconds': remaining_time,
                    'will_shutdown_at': time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(timer_value + 300)),
                    'shutdown_flag': self.shutdown_flag
                })
            else:
                return jsonify({
                    'timer_initialized': False,
                    'worker_pid': os.getpid(),
                    'message': 'Timer not initialized yet'
                })

        @self.app.route('/pki/ca', methods=['GET'])
        @openapi_validated
        def get_pki():
            ret = executor.get_pki()
            print(ret)
            if ret:
                return jsonify(ret), 200
            else:
                return '', 404

        @self.app.route('/pki/ca/create', methods=['POST'])
        @openapi_validated
        def create_pki():
            def run():
                return executor.create_pki(**request.openapi.body)
            ret = self._stream_execution_logs(run)
            print(ret)
            return jsonify(ret), 200

        @self.app.route('/pki/ca/import', methods=['POST'])
        @openapi_validated
        def import_pki():
            def run():
                return executor.import_pki(**request.openapi.body)
            self._stream_execution_logs(run)
            print('import_pki completed')
            return '', 200

        @self.app.route('/pki/certificate', methods=['GET'])
        @openapi_validated
        def get_certificate():
            def run():
                return executor.get_certificate(**request.openapi.parameters.query)
            ret = self._stream_execution_logs(run)
            print(ret)
            if ret:
                return jsonify(ret), 200
            else:
                return '', 404

        @self.app.route('/pki/certificate/host', methods=['POST'])
        @openapi_validated
        def create_certificate_on_host():
            def run():
                return executor.create_certificate_on_host(**request.openapi.body)
            ret = self._stream_execution_logs(run)
            print(ret)
            return jsonify(ret), 200

        @self.app.route('/pki/certificate/device', methods=['POST'])
        @openapi_validated
        def create_certificate_on_device():
            def run():
                return executor.create_certificate_on_device(**request.openapi.body)
            self._stream_execution_logs(run)
            print('create_certificate_on_device completed')
            return '', 200

        @self.app.route('/device/info', methods=['POST'])
        @openapi_validated
        def get_device():
            def run():
                return executor.get_device(**request.openapi.body)
            ret = self._stream_execution_logs(run)
            print(ret)
            return jsonify(ret), 200

        @self.app.route('/device/storage', methods=['POST'])
        @openapi_validated
        def store_to_device():
            def run():
                return executor.store_to_device(**request.openapi.body)
            self._stream_execution_logs(run)
            print('store_to_device completed')
            return '', 200

    def _check_server_status(self):
        """Background thread to monitor server status"""
        while not self.shutdown_flag:
            time.sleep(5)
            pid_info = self.read_pid_file()
            if pid_info and self.is_process_running(pid_info['pid']):
                # Update start time if server is running
                self.app.start_time = pid_info['start_time']
            else:
                # If server is not running, reset start time
                self.app.start_time = time.time()

    def _watchdog_timer(self):
        """Background thread that monitors the internal timer and shuts down server after 5 minutes of inactivity"""
        while not self.shutdown_flag:
            time.sleep(10)  # Check every 10 seconds

            timer_value = self.get_internal_timer()
            if timer_value and not self.shutdown_flag:
                current_time = time.time()
                time_elapsed = current_time - timer_value

                print(f"[DEBUG] Worker {os.getpid()}: Watchdog check: {time_elapsed:.1f} seconds since last timer reset")

                if time_elapsed >= 300:  # 5 minutes = 300 seconds
                    print(f"[{time.strftime('%Y-%m-%d %H:%M:%S')}] Worker {os.getpid()}: Internal timer expired after 5 minutes, shutting down server...")
                    self.shutdown_flag = True
                    self.stop()

    def write_pid_file(self, pid):
        """Write the process ID to a file"""
        try:
            with open(self.pid_file, 'w') as f:
                json.dump({
                    'pid': pid,
                    'start_time': time.time(),
                    'host': self.host,
                    'port': self.port
                }, f)
            return True
        except Exception as e:
            print(f"Error writing PID file: {e}")
            return False

    def read_pid_file(self):
        """Read the process ID from file"""
        try:
            if self.pid_file.exists():
                with open(self.pid_file, 'r') as f:
                    return json.load(f)
        except Exception:
            pass
        return None

    def remove_pid_file(self):
        """Remove the PID file"""
        try:
            if self.pid_file.exists():
                self.pid_file.unlink()
            # Also cleanup the timer file
            if self.timer_file.exists():
                self.timer_file.unlink()
            return True
        except Exception as e:
            print(f"Error removing PID file: {e}")
            return False

    def get_internal_timer(self):
        """Get the internal timer timestamp from shared file"""
        try:
            if self.timer_file.exists():
                with open(self.timer_file, 'r') as f:
                    return float(f.read().strip())
        except (ValueError, FileNotFoundError, OSError):
            pass
        return None

    def set_internal_timer(self, timestamp=None):
        """Set the internal timer timestamp in shared file"""
        if timestamp is None:
            timestamp = time.time()
        try:
            with open(self.timer_file, 'w') as f:
                f.write(str(timestamp))
            return timestamp
        except OSError:
            return None

    def is_process_running(self, pid):
        """Check if a process is running"""
        try:
            if sys.platform == "win32":
                # Windows
                result = subprocess.run(['tasklist', '/FI', f'PID eq {pid}'],
                                      capture_output=True, text=True)
                return str(pid) in result.stdout
            else:
                # Unix-like systems
                os.kill(pid, 0)
                return True
        except (OSError, subprocess.SubprocessError):
            return False

    def is_port_available(self, host=None, port=None):
        """Check if a port is available for binding"""
        host = host or self.host
        port = port or self.port

        try:
            # First check if we can bind to the specific host:port
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                sock.bind((host, port))

            # Also check if anything is listening on 0.0.0.0:port (all interfaces)
            # which would conflict with our binding
            if host != '0.0.0.0':
                try:
                    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                        # Try to connect to see if something is listening on all interfaces
                        sock.settimeout(1)
                        result = sock.connect_ex((host, port))
                        if result == 0:
                            # Something is listening and accepting connections
                            return False
                except:
                    pass

            return True
        except (socket.error, OSError):
            return False

    def find_available_port(self, start_port=None, max_attempts=10):
        """Find an available port starting from start_port"""
        start_port = start_port or self.port
        for port in range(start_port, start_port + max_attempts):
            if self.is_port_available(self.host, port):
                return port
        return None

    def get_port_usage_info(self, host=None, port=None):
        """Get information about what's using a specific port"""
        host = host or self.host
        port = port or self.port

        try:
            if sys.platform == "win32":
                # Windows - use netstat to find what's using the port
                result = subprocess.run(['netstat', '-ano'], capture_output=True, text=True)
                lines = result.stdout.split('\n')
                for line in lines:
                    if f':{port}' in line and 'LISTENING' in line:
                        parts = line.split()
                        if len(parts) >= 5:
                            pid = parts[-1]
                            # Try to get process name
                            try:
                                proc_result = subprocess.run(['tasklist', '/FI', f'PID eq {pid}'],
                                                           capture_output=True, text=True)
                                proc_lines = proc_result.stdout.split('\n')
                                for proc_line in proc_lines:
                                    if pid in proc_line:
                                        proc_name = proc_line.split()[0]
                                        return f"Process: {proc_name} (PID: {pid})"
                            except:
                                pass
                            return f"PID: {pid}"
            else:
                # Unix-like systems - use lsof or netstat
                try:
                    result = subprocess.run(['lsof', '-i', f':{port}'], capture_output=True, text=True)
                    if result.stdout:
                        lines = result.stdout.strip().split('\n')
                        if len(lines) > 1:  # Skip header
                            parts = lines[1].split()
                            if len(parts) >= 2:
                                return f"Process: {parts[0]} (PID: {parts[1]})"
                except FileNotFoundError:
                    # lsof not available, try netstat
                    result = subprocess.run(['netstat', '-tlnp'], capture_output=True, text=True)
                    lines = result.stdout.split('\n')
                    for line in lines:
                        if f':{port} ' in line and 'LISTEN' in line:
                            parts = line.split()
                            if len(parts) >= 7:
                                proc_info = parts[-1]
                                return f"Process info: {proc_info}"
        except Exception:
            pass
        return "Unknown process"

    def _wait_for_server_start(self, timeout_sec=30, poll_interval_sec=1):
        """Poll until an HTTP request succeeds or timeout."""
        probe_host = '127.0.0.1' if self.host in ('0.0.0.0', '::', '[::]') else self.host
        url = f'http://{probe_host}:{self.port}/'
        deadline = time.monotonic() + timeout_sec
        while time.monotonic() < deadline:
            try:
                with urllib.request.urlopen(url, timeout=2.0) as resp:
                    if resp.getcode() == 200:
                        return True
            except (urllib.error.URLError, OSError):
                pass
            time.sleep(poll_interval_sec)
        return False

    def start(self, force=False):
        """Start the web server"""
        # Check if our server is already running
        pid_info = self.read_pid_file()
        if pid_info and self.is_process_running(pid_info['pid']):
            print(f"Server is already running (PID: {pid_info['pid']})")
            print(f"Access it at: http://{pid_info['host']}:{pid_info['port']}")
            return False

        # Remove stale PID file
        self.remove_pid_file()

        # Check if the requested port is available (unless forced)
        if not force and not self.is_port_available():
            port_usage = self.get_port_usage_info()
            print(f"⚠️  Port {self.port} is already in use (used by: {port_usage})")

            # Automatically find and use an alternative port
            alternative_port = self.find_available_port(self.port + 1)
            if alternative_port:
                print(f"🔄 Automatically switching to port {alternative_port}")
                self.port = alternative_port
            else:
                print("❌ No alternative ports found in the range")
                print("   Try specifying a different port with --port <number>")
                print("💡 Tip: Use --force to override port protection")
                return False
        elif force and not self.is_port_available():
            port_usage = self.get_port_usage_info()
            print(f"⚠️  Warning: Port {self.port} is busy (used by: {port_usage})")
            print(f"🔧 Force mode enabled - attempting to start anyway...")

        print(f"Starting production server on {self.host}:{self.port}...")

        # Initialize internal timer using shared file (for parent process)
        timer_value = self.set_internal_timer()
        self.shutdown_flag = False
        print(f"[DEBUG] Parent process: Internal timer initialized at {timer_value}")
        print("[DEBUG] Watchdog timers will be started in each worker process")

        # Start server in background
        script_path = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), 'server.py')
        cmd = [
            sys.executable, script_path, '_run_server',
            '--host', self.host, '--port', str(self.port),
            '--cwd', os.path.abspath(self.cwd),
        ]
        # Do not pass any open file descriptors to the server process
        popen_kwargs = {
            'cwd': self.cwd,
            'stdin': subprocess.DEVNULL,
            'stdout': subprocess.DEVNULL,
            'stderr': subprocess.DEVNULL,
            'close_fds': True,
        }
        if sys.platform == "win32":
            popen_kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
        else:
            popen_kwargs['start_new_session'] = True
        try:
            subprocess.Popen(cmd, **popen_kwargs)
        except OSError as exc:
            print(f"❌ Failed to start server process: {exc}")
            return False

        if not self._wait_for_server_start():
            print("❌ Failed to start server process (timeout)")
            if force:
                print("   This might be due to the port conflict that was ignored")
            return False

        print(f"✅ Server started successfully")
        print(f"🌐 Access it at: http://{self.host}:{self.port}")
        # This is the trigger line for the UI
        print(
            f"WISUN_DDP_READY|{{\"host\":\"{self.host}\",\"port\":{self.port}}}", flush=True)
        return True

    def _run_server_process(self, stdout=False):
        """Run the production server process"""
        # Redirect stdout/stderr to log file for background operation
        if not stdout:
            log_file = open(self.log_file, mode='a', buffering=1)
            sys.stdout = log_file
            sys.stderr = log_file

        self.app.start_time = time.time()

        # Initialize timer in the worker process
        self.set_internal_timer()

        # Start the watchdog thread in each worker process
        self.watchdog_thread = threading.Thread(target=self._watchdog_timer, daemon=True)
        self.watchdog_thread.start()
        print(f"[DEBUG] Worker {os.getpid()}: Watchdog timer started - will shutdown after 5 minutes of inactivity")

        # Write PID file
        self.write_pid_file(os.getpid())

        # Setup signal handlers for graceful shutdown
        def signal_handler(signum, frame):
            print(f"\nReceived signal {signum}, shutting down...")
            self.shutdown_flag = True
            self.remove_pid_file()
            sys.exit(0)

        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)

        try:
            # Use production WSGI server with SocketIO support
            try:
                if sys.platform == "win32":
                    # Use Waitress for Windows (and as fallback)
                    from waitress import serve
                    print(f"Starting production server (Waitress) with WebSocket support on {self.host}:{self.port}")
                    serve(self.app, host=self.host, port=self.port)
                else:
                    # Try eventlet-based SocketIO server for Unix systems, fallback to Waitress
                    try:
                        # Use SocketIO's built-in server with eventlet for better WebSocket performance
                        print(f"Starting production server with WebSocket support on {self.host}:{self.port}")

                        # Use SocketIO's production server
                        self.socketio.run(
                            self.app,
                            host=self.host,
                            port=self.port,
                            debug=False,
                            use_reloader=False
                        )


                    except Exception as e:
                        print(f"SocketIO server failed: {e}")
                        # Fallback to Waitress (WebSocket will use polling)
                        from waitress import serve
                        print(f"Falling back to Waitress server on {self.host}:{self.port}")
                        print("Note: WebSocket will use polling fallback")
                        serve(self.socketio, host=self.host, port=self.port)
            except ImportError as e:
                print(f"Error: Production server not available: {e}")
                print("Please install dependencies with: pip install eventlet")
                print("Exiting...")
                sys.exit(1)
        finally:
            self.remove_pid_file()

    def stop(self):
        """Stop the web server"""
        print("Stopping server...")

        # Set shutdown flag to stop watchdog thread
        self.shutdown_flag = True

        pid_info = self.read_pid_file()

        if not pid_info:
            print("No server appears to be running (no PID file found)")
            return False

        pid = pid_info['pid']

        if not self.is_process_running(pid):
            print(f"Server with PID {pid} is not running")
            self.remove_pid_file()
            return False

        print(f"Stopping server (PID: {pid})...")

        try:
            if sys.platform == "win32":
                # Windows
                subprocess.run(['taskkill', '/F', '/PID', str(pid)], check=True)
            else:
                # Unix-like systems
                os.kill(pid, signal.SIGTERM)

                # Wait for graceful shutdown
                for _ in range(10):
                    if not self.is_process_running(pid):
                        break
                    time.sleep(0.5)

                # Force kill if still running
                if self.is_process_running(pid):
                    os.kill(pid, signal.SIGKILL)

            self.remove_pid_file()
            print("Server stopped successfully")
            return True

        except Exception as e:
            print(f"Error stopping server: {e}")
            return False

    def check_port(self, host=None, port=None):
        """Check if a port is available"""
        host = host or self.host
        port = port or self.port

        print(f"Checking port {port} on {host}...")

        if self.is_port_available(host, port):
            print(f"✅ Port {port} is available")
            return True
        else:
            port_usage = self.get_port_usage_info(host, port)
            print(f"❌ Port {port} is busy")
            print(f"   Used by: {port_usage}")

            # Suggest alternatives
            print(f"\n🔍 Looking for alternative ports...")
            alternatives = []
            for i in range(1, 11):
                alt_port = port + i
                if self.is_port_available(host, alt_port):
                    alternatives.append(alt_port)
                    if len(alternatives) >= 3:  # Show up to 3 alternatives
                        break

            if alternatives:
                print(f"💡 Available alternatives: {', '.join(map(str, alternatives))}")
            else:
                print("❌ No nearby alternatives found")

            return False

    def status(self):
        """Check server status"""
        pid_info = self.read_pid_file()

        if not pid_info:
            print("Status: Not running (no PID file)")
            return False

        pid = pid_info['pid']

        if self.is_process_running(pid):
            uptime = time.time() - pid_info['start_time']
            print(f"Status: Running")
            print(f"PID: {pid}")
            print(f"Host: {pid_info['host']}")
            print(f"Port: {pid_info['port']}")
            print(f"Uptime: {uptime:.1f} seconds")
            print(f"URL: http://{pid_info['host']}:{pid_info['port']}")

            # Also check if the port is still available (someone else might have taken it)
            if not self.is_port_available(pid_info['host'], pid_info['port']):
                print("✅ Port is properly bound")
            else:
                print("⚠️  Warning: Port appears to be available (server might not be listening)")

            return True
        else:
            print(f"Status: Not running (PID {pid} not found)")
            self.remove_pid_file()
            return False

    def restart(self, force=False):
        """Restart the web server"""
        print("Restarting server...")
        self.stop()
        time.sleep(1)
        return self.start(force)


# For direct execution
if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='HTTP Server Service')
    parser.add_argument('command', choices=['_run_server'], help='Internal command')
    parser.add_argument('--host', default=DEFAULT_HOST)
    parser.add_argument('--port', type=int, default=DEFAULT_PORT)

    args = parser.parse_args()

    if args.command == '_run_server':
        server = HttpServer(args.host, args.port)
        server._run_server_process(stdout=True)
