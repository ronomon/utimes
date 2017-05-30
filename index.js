'use strict';

var MAXTIME = Math.pow(2, 48) - 1;

var Node = {
  fs: require('fs'),
  path: require('path'),
  process: process
};

var binding = require('./binding.node');

function assertFunction(key, value) {
  if (typeof value !== 'function') {
    throw new Error(key + ' must be a function');
  }
}

function assertPath(key, value) {
  if (typeof value !== 'string') {
    throw new Error(key + ' must be a string');
  }
  if (value.length === 0) {
    throw new Error(key + ' must not be empty');
  }
  if (value.indexOf('\u0000') !== -1) {
    throw new Error(key + ' must be a string without null bytes');
  }
}

function assertTime(key, value) {
  if (value === undefined) return;
  if (typeof value !== 'number') {
    throw new Error(key + ' must be a number or undefined');
  }
  if (Math.floor(value) !== value) {
    throw new Error(key + ' must be an integer');
  }
  if (value < 0) {
    throw new Error(key + ' must be a positive integer');
  }
  if (value > MAXTIME) {
    throw new Error(key + ' must not be more than ' + MAXTIME);
  }
}

function pathBuffer(path) {
  var pathLong = Node.path._makeLong(path);
  var buffer = Buffer.alloc(Buffer.byteLength(pathLong, 'utf-8') + 1);
  buffer.write(pathLong, 0, buffer.length - 1, 'utf-8');
  buffer[buffer.length - 1] = 0;
  if (buffer.indexOf(0) !== buffer.length - 1) {
    throw new Error('path must be a string without null bytes');
  }
  return buffer;
}

var Utimes = {};

Utimes.utimes = function(path, btime, mtime, atime, callback) {
  assertPath('path', path);
  assertTime('btime', btime);
  assertTime('mtime', mtime);
  assertTime('atime', atime);
  assertFunction('callback', callback);
  var flags = 0;
  if (btime === undefined) {
    btime = 0;
  } else {
    flags |= 1;
  }
  if (mtime === undefined) {
    mtime = 0;
  } else {
    flags |= 2;
  }
  if (atime === undefined) {
    atime = 0;
  } else {
    flags |= 4;
  }
  if (flags === 0) return callback();
  if (Node.process.platform === 'darwin' || Node.process.platform === 'win32') {
    binding.utimes(pathBuffer(path), flags, btime, mtime, atime,
      function(result) {
        if (result !== 0) {
          var error = new Error('(' + result + '), utimes(' + path + ')');
          return callback(error);
        }
        callback();
      }
    );
  } else if (flags === 1) {
    // No need to set mtime or atime.
    // Linux does not support btime.
    callback();
  } else if (flags & 2) {
    // We must set mtime.
    // If atime is undefined we make it the same as mtime rather than 0.
    if (!(flags & 4)) atime = mtime;
    Node.fs.utimes(path, atime / 1000, mtime / 1000, callback);
  } else {
    // We must set atime without changing mtime.
    Node.fs.stat(path,
      function(error, stats) {
        if (error) return callback(error);
        mtime = stats.mtime.getTime();
        Node.fs.utimes(path, atime / 1000, mtime / 1000, callback);
      }
    );
  }
};

module.exports = Utimes;

// S.D.G.
