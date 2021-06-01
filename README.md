# LScript

LScript is designed to be an extension to existing applications or APIs to allow for scripting in those projects. Currently, the project is in a state in which it would be unpractical to script in due to the verbose and nontrivial syntax. However, the project is actively being developed and new features are being added.

## Features

- Object oriented
- Garbage collected
- Several library classes included
- Runtime linking of LScript functions to native functions

## Building

Currently, LScript is only avaliable for 64-bit Windows and is built using Visual Studio 2019. It can be built inside the solution `lscript.sln`.

## Starting the Virtual Machine

There exist three functions relevant to starting the virtual machine, declared in `lscript.h` in the `lscriptlib` project. These are:

`lint ls_init()`
- Performs initialization, returns `0` on success.

`LVM ls_create_vm(int argc, const char *const argv[], void *lsAPILib)`
- Creates the virtual machine with the given arguments.
- `argc` - The number of elements in `argv`.
- `argv` - An array of strings to be the virtual-machine arguments.
- `lsAPILib` - A pointer to a handle in which the virtual machine is stored. For Windows, this is an `HMODULE` (Generally referencing `lscriptapi.dll`).

`int ls_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID)`
- Starts the virtual machine by running a script with the given arguments, optionally on a separate thread.
- `argc` - The number of elements in `argv`.
- `argv` - First element is the class name of a script on the classpath to run. The class should contain a `static` function with the name `main` taking an `objectarray` of `String`'s. The rest of the elements in the array will be the arguments passed to the `main` function.
- `threadHandle` - A pointer to a pointer to a thread handle which will contain the thread created to run the virtual machine on. If `NULL`, no new thread will be created and the virtual machine will run on the thread in which this function was called.
- `threadID` - A pointer to an `unsigned long` which will contain the ID of the thread created to run the virtual machine on, if one was started. Can be `NULL`.

`LVM ls_create_and_start_vm(int argc, const char *const argv[], void **threadHandle, unsigned long *threadID)`
- A combination of `ls_create_vm` and `ls_start_vm`, called in that order.
- `argc` - The number of elements in `argv`.
- `argv` - A merged array of the arguments in `ls_create_vm` and `ls_start_vm`. All arguments should be individual elements. Arguments passed to `ls_create_vm` should be in the first part of the array and all arguments to `ls_start_vm` should be in the second part.
- `threadHandle` - A pointer to a pointer to a thread handle which will contain the thread created to run the virtual machine on. If `NULL`, no new thread will be created and the virtual machine will run on the thread in which this function was called.
- `lsAPILib` - A pointer to a handle in which the virtual machine is stored. For Windows, this is an `HMODULE` (Generally referencing `lscriptapi.dll`).

These can be used when debugging in Visual Studio by setting the Debug Command Arguments when running the `lscript` project.

### Virtual Machine Creation Arguments

Below are a valid list of arguments which can be passed to `ls_create_vm`:

- `-verbose` - Indicates verbose output should be printed.
- `-nodebug` - Indicates no debugging symbols should be loaded, if any are found.
- `-verr` - Similar to `-verbose`, but only errors are printed. No effect if `-verbose` was specified.
- `-path [directory]` - Specifies a directory to add to the classpath.

### Virtual Machine Start Arguments

- `[classname]` - The name of a class on the classpath on which to start the virtual machine. Must contain a 
- `...` - The arguments to be passed to the `main` function. Each argument is an individual element in the array. The class should contain a `static` function with the name `main` taking an `objectarray` of `String`'s.

### Stopping the Virtual Machine

The virtual machine can be stopped with:

`lvoid ls_destroy_vm(unsigned long threadWaitTime)`
- `threadWaitTime` - The time to wait for the virtual machine thread to finish before stopping it, in milliseconds. Only affects virtual machines started on a separate thread.