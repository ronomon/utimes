# utimes
Change the birth time (`btime`), modified time (`mtime`) and/or access time (`atime`) of a file on Windows, macOS and Linux.

## Installation
```
npm install @ronomon/utimes
```

## Usage
Timestamps `btime`, `mtime`, `atime` should be passed as Unix millisecond timestamps. Timestamps which are `undefined` will not be changed. Linux does not support setting the `btime` timestamp. File descriptors are not supported at present.

```javascript
var Utimes = require('@ronomon/utimes');
var btime = 447775200000; // 1984-03-10T14:00:00.000Z
var mtime = undefined;
var atime = undefined;
Utimes.utimes(path, btime, mtime, atime, callback);
```

## Tests
```
node test.js
```
