cub
===

cub is a statically typed language.

Quickstart
----------

Install [gyp](https://code.google.com/p/gyp/) for building, and [llvm](http://llvm.org/) for compiling.

Compile and run the fibonacci test program, which prints out the [8th fibonacci number](http://wolfr.am/63QUgOux):

```
$ gyp -f make --depth=.
$ make
$ ./cub test/test.cub test/test
$ ./test/test
21
```

The compile process is still kinda clunky and runs through a bash script.

License
-------

> The MIT License (MIT)
>
> Copyright &copy; 2015 Colby Skeggs and Eli Skeggs
>
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
