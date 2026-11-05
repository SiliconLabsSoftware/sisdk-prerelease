
"""
A Python replacement for java.util.Properties class
This is modelled as closely as possible to the Java original.

Created - Anand B Pillai <abpillai@gmail.com>
"""

import sys
import io
import re
import time

__all__ = ['IllegalArgumentException', 'Properties']

class IllegalArgumentException(Exception):

    def __init__(self, lineno, msg):
        self.lineno = lineno
        self.msg = msg

    def __str__(self):
        s='Exception at line number %d => %s' % (self.lineno, self.msg)
        return s

class Properties(object):
    """ A Python replacement for java.util.Properties """

    def __init__(self, props=None):


        self._props = {}
        self._origprops = {}
        self._keyorder = []
        self._keymap = {}

        self.othercharre = re.compile(r'(?<!\\)(\s*\=)|(?<!\\)(\s*\:)')
        self.othercharre2 = re.compile(r'(\s*\=)|(\s*\:)')
        self.bspacere = re.compile(r'\\(?!\s$)')

    def __str__(self):
        s='{'
        for key,value in list(self._props.items()):
            s = ''.join((s,key,'=',value,', '))

        s=''.join((s[:-2],'}'))
        return s

    def __parse(self, lines):
        """ Parse a list of lines and create
        an internal property dictionary """






        lineno=0
        i = iter(lines)

        for line in i:
            lineno += 1
            line = line.strip()
            if not line: continue
            if line[0] in ('#','!'): continue
            escaped=False
            sepidx = -1
            flag = 0
            m = self.othercharre.search(line)
            if m:
                first, last = m.span()
                start, end = 0, first
                flag = 1
                wspacere = re.compile(r'(?<![\\\=\:])(\s)')
            else:
                if self.othercharre2.search(line):

                    wspacere = re.compile(r'(?<![\\])(\s)')
                start, end = 0, len(line)

            m2 = wspacere.search(line, start, end)
            if m2:
                first, last = m2.span()
                sepidx = first
            elif m:
                first, last = m.span()
                sepidx = last - 1


            while line[-1] == '\\':
                nextline = next(i)
                nextline = nextline.strip()
                lineno += 1
                line = line[:-1] + nextline

            if sepidx != -1:
                key, value = line[:sepidx], line[sepidx+1:]
            else:
                key,value = line,''
            self._keyorder.append(key)
            self.processPair(key, value)
      
    def _isPy3(self):
        return sys.version > '3'
    
    def _isFileType(self, arg):
        if self._isPy3():
            return type(arg) is io.TextIOWrapper
        else:
            return type(arg) is file  

    def processPair(self, key, value):
        """ Process a (key, value) pair """

        oldkey = key
        oldvalue = value

        keyparts = self.bspacere.split(key)

        strippable = False
        lastpart = keyparts[-1]

        if lastpart.find('\\ ') != -1:
            keyparts[-1] = lastpart.replace('\\','')

        elif lastpart and lastpart[-1] == ' ':
            strippable = True

        key = ''.join(keyparts)
        if strippable:
            key = key.strip()
            oldkey = oldkey.strip()

        oldvalue = self.unescape(oldvalue)
        value = self.unescape(value)

        curlies = re.compile("{.+?}")
        found = curlies.findall(value)

        for f in found:
            srcKey = f[1:-1]
            if srcKey in self._props:
                value = value.replace(f, self._props[srcKey], 1)

        self._props[key] = value.strip()

        if key in self._keymap:
            oldkey = self._keymap.get(key)
            self._origprops[oldkey] = oldvalue.strip()
        else:
            self._origprops[oldkey] = oldvalue.strip()
            self._keymap[key] = oldkey

        if key not in self._keyorder:
            self._keyorder.append(key)

    def escape(self, value):

        newvalue = value.replace(':','\:')
        newvalue = newvalue.replace('=','\=')

        return newvalue

    def unescape(self, value):

        newvalue = value.replace('\:',':')
        newvalue = newvalue.replace('\=','=')

        return newvalue

    def load(self, stream):
        """ Load properties from an open file stream """

        if not self._isFileType(stream):
            raise TypeError('Argument should be a file object!')
        if stream.mode != 'r':
            raise ValueError('Stream should be opened in read-only mode!')

        try:
            lines = stream.readlines()
            self.__parse(lines)
        except IOError:
            raise

    def getProperty(self, key):
        """ Return a property for the given key """

        return self._props.get(key,'')

    def setProperty(self, key, value):
        """ Set the property for the given key """

        if type(key) is str and type(value) is str:
            self.processPair(key, value)
        else:
            raise TypeError('both key and value should be strings!')

    def propertyNames(self):
        """ Return an iterator over all the keys of the property
        dictionary, i.e the names of the properties """

        return list(self._props.keys())

    def list(self, out=sys.stdout):
        """ Prints a listing of the properties to the
        stream 'out' which defaults to the standard output """

        out.write('-- listing properties --\n')
        for key,value in list(self._props.items()):
            out.write(''.join((key,'=',value,'\n')))

    def store(self, out, header=""):
        """ Write the properties list to the stream 'out' along
        with the optional 'header' """

        if out.mode[0] != 'w':
            raise ValueError('Steam should be opened in write mode!')

        try:
            out.write(''.join(('#',header,'\n')))
            tstamp = time.strftime('%a %b %d %H:%M:%S %Z %Y', time.localtime())
            out.write(''.join(('#',tstamp,'\n')))
            for prop in self._keyorder:
                if prop in self._origprops:
                    val = self._origprops[prop]
                    out.write(''.join((prop,'=',self.escape(val),'\n')))

            out.close()
        except IOError:
            raise

    def getPropertyDict(self):
        return self._props

    def __getitem__(self, name):
        """ To support direct dictionary like access """

        return self.getProperty(name)

    def __setitem__(self, name, value):
        """ To support direct dictionary like access """

        self.setProperty(name, value)

    def __getattr__(self, name):
        """ For attributes not found in self, redirect
        to the properties dictionary """

        try:
            return self.__dict__[name]
        except KeyError:
            if hasattr(self._props,name):
                return getattr(self._props, name)
