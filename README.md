# StarC
### *Project is in development: Expect things to break catastrophically*


## Introduction
Add defers to C by parsing the text with the starcc executable produced \
It handles most cases correctly to the extent of my knowledge.

## Build
Currently, no releases are provided and you will have to build the project from source.

You can either do this using the makefile provided or compile file yourself, as the project has no dependancies other than libc:

Simply compile the star.c file, including the include/ folder and link them:
`gcc -Iinclude/ src/star.c -o build/starcc`

Adjust paths and directories to your requirements.

## Usage
Pass in the file you want to parse and the output file (stdout if unspecified):
```./build/starcc helloWorld.c -o build/helloWorld_parsed.c```