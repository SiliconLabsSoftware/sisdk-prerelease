#!/usr/bin/env python3
"""
WebSocket Service Module

Contains WebSocket functionality for real-time communication.
"""

import time
from flask_socketio import SocketIO, emit


class WebSocketService:
    """WebSocket service management class"""

    def __init__(self, app, common_executor, shutdown_flag_ref):
        self.app = app
        self.common_executor = common_executor
        self.shutdown_flag_ref = shutdown_flag_ref

        # Initialize SocketIO with WebSocket-only transport
        self.socketio = SocketIO(
            self.app,
            cors_allowed_origins="*",
            transport=['websocket', 'polling'],  # Allow both transports for better compatibility
            engineio_logger=True,  # Enable logging to debug connection issues
            socketio_logger=True,
            async_mode='eventlet',  # Use eventlet for better production performance
            ping_interval=120,  # Send ping every 2 minutes (instead of 25 seconds)
            ping_timeout=60,    # Wait 1 minute for pong response (instead of 20 seconds)
            allow_upgrades=True
        )

        self._setup_websocket_handlers()

    def _setup_websocket_handlers(self):
        """Setup WebSocket event handlers"""

        @self.socketio.on('connect')
        def handle_connect():
            """Handle WebSocket client connection"""
            print("WebSocket client connected")
            emit('connection_status', {'status': 'connected', 'message': 'Connected to WiSUN Applications Server'})

        @self.socketio.on('disconnect')
        def handle_disconnect():
            """Handle WebSocket client disconnection"""
            print("WebSocket client disconnected")

        @self.socketio.on('message')
        def handle_message(data):
            """Handle general messages from clients"""
            print(f"Received message: {data}")
            # Echo back or broadcast to all clients
            emit('message', {'received': data, 'timestamp': time.time()})

    def send_message(self, event, message):
        """Send a message to all connected WebSocket clients"""
        self.socketio.emit(event, message)

