# LibSIG

LibSIG is a set of tools to extract library calls from a program in order to obtain a signature of its execution.
This is useful for binary diffing, a technique to determine if two binary programs originate from the same source code.
Therefore, LibSIG can be used to identify malware, plagiarism, or code theft.

LibSIG has two main implementations: one using the rtld-audit API and another as a plugin using Valgrind's infrastructure.
The rtld-audit version is fast, but it requires that presence of the PLT (Procedure Linkage Table) in the binary to be able to track its library calls.
If the PLT is not present, then the Valgrind plugin version is more suitable.
Note that, although the Valgrind version is slower than the rtld-audit version, it has a small footprint -- its overhead is similar to running Valgrind with no plugins.
Both strategies will be discussed below.

## Using rtld-audit

Compile the rtld-audit library that will be used to perform the library signature.

    $ cd audit
    $ gcc -fPIC -shared -o libsig.so libsig.c

Compile a test program to check its signature.
It does not matter if the binary uses protection mechanisms such as address space layout
randomization (ASLR) or is stripped of its debugging symbols.

    $ cd ../tests
    $ gcc -o bubblesort bubblesort.c
    $ strip -s bubblesort

To extract the signature, first setup two environment variables: LD_AUDIT to load the shared library and LIBSIG_OUTPUT to output the signature to a file (use %p to append the pid of the process to the name of the output file if used in concurrent programs).
And then execute the program with its arguments.

    $ LD_AUDIT=../audit/libsig.so LIBSIG_OUTPUT=bubblesort.signature ./bubblesort 15 4 8 16 42 23
    4 8 15 16 23 42

The verify the signature, just read the signature file extracted.
This file contains lines with two columns separated by commas:
the first column is the address of the implementation of the function in the loaded library address space, whereas the second column is the name of the function called.

    $ cat bubblesort.signature
    0x146dfa0050e0,malloc
    0x146df9faf5b0,atoi
    0x146df9faf5b0,atoi
    0x146df9faf5b0,atoi
    0x146df9faf5b0,atoi
    0x146df9faf5b0,atoi
    0x146df9faf5b0,atoi
    0x146df9fccc90,printf
    0x146df9fccc90,printf
    0x146df9fccc90,printf
    0x146df9fccc90,printf
    0x146df9fccc90,printf
    0x146df9fccc90,printf
    0x146df9ff1280,putchar
    0x146dfa0056d0,free

In this example, we can see that this example program calls `malloc` and `free` once, converts strings to numbers via `atoi` 6 times, prints the numbers using `printf` 6 times and ends with a call to `putchar` to print the end of line.

> [!WARNING]
> The plugin was designed to avoid overwriting the contents of the output file is it already exists. Thus, in multiple executions, if the %p was not used in the filename, remove the file before executing it.

Although fast, the rtld-audit version has shortcomings when the binary does not use PLT. Consider the following example:

    $ gcc -o squareroot squareroot.c -ldl
    $ strip -s squareroot
    $ LD_AUDIT=../audit/libsig.so LIBSIG_OUTPUT=squareroot.signature ./squareroot 16
    √16 = 4
    $ cat squareroot.signature
    0x14fad14d55b0,atoi
    0x14fad1684390,dlopen
    0x14fad15f1be0,_dl_catch_error
    0x14fad16844b0,dlsym
    0x14fad15f1be0,_dl_catch_error
    0x14fad15f1520,_dl_sym
    0x14fad18b2490,_dl_find_dso_for_object
    0x14fad1684440,dlclose
    0x14fad15f1be0,_dl_catch_error
    0x14fad14f2c90,printf

> [!NOTE]  
> The call to sqrt was missed because the math library was loaded and linked at runtime. The calls to `atoi` and `printf` could also be lost if this example was compiled without a PLT by using the compiler flag `-fno-plt`.

## Using Valgrind

LibSIG has a Valgrind plugin version that can track when the code crosses boundaries from a code section, the text section of a program for example, to somewhere outside of it, a library code for example.
Thus, this can also be used as a runtime signature for a program.

To use LibSIG's Valgrind plugin version, first return back to the project's root directory (if you are in the tests directory by following the rtld-audit instructions) and download and unpack Valgrind (3.25.1).

    $ cd ../
    $ wget -qO - https://sourceware.org/pub/valgrind/valgrind-3.25.1.tar.bz2 | tar jxv

Enter Valgrind's base directory, copy the plugin implementation and apply a patch to add the tool to its compilation chain.

    $ cd valgrind-3.25.1
    $ cp -R ../plugin libsig
    $ patch -p1 < libsig/libsig.patch

Build and install Valgrind with LibSIG.

    $ ./autogen.sh
    $ ./configure
    $ make -j4
    $ sudo make install

Now, let's try the squareroot example again, this time even
without using the PLT (by using the `-fno-plt` flag) and also without debugging symbols.

    $ cd ../tests
    $ gcc -fno-plt -o squareroot squareroot.c -ldl
    $ strip -s squareroot

Use the [libsig_symbols](plugin/libsig_symbols) to extract symbols names from the program regardless of the presence of debugging symbols.
If the program has no PLT, this tool will extract its symbols from the GOT, otherwise it will use the names in the PLT itself.

    $ libsig_symbols squareroot > squareroot.symbols

Now load the symbols in the LibSIG tool to record the libraries signature of this program.
In this example, we are coalescing the output if the same function is called multiple times in a row.
Note that the Valgrind plugin will overwrite the contents of the output file is it exists, as opposed to the rtld-audit version that just ignores it.

    $ valgrind -q --tool=libsig --symbols=squareroot.symbols --records=squareroot.signature --coalesce=yes -- ./squareroot 16
    4 8 15 16 23 42

Since this plugin supports multithreaded programs, the output file contains a commented line with the thread id followed by the recorded boundaries crosses with its address and how many times it was crossed consecutively.
By default, the plugin considers the text section of the instrumentation program as the address range to track, but this can be changed by the `--bound` command line argument. This option can be used multiple times if necessary.

    $ cat squareroot.signature
    # Thread: 1
    0x4006100,???,1
    0x487ef90,(below main),1
    0x4001000,???,1
    0x487f010,(below main),1
    0x489f5b0,atoi,1
    0x4856390,dlopen@@GLIBC_2.2.5,1
    0x48564b0,dlsym,1
    0x4a60780,sqrt,1
    0x4856440,dlclose,1
    0x48bcc90,printf,1
    0x487f083,(below main),1
    0x4001030,???,1
    0x4016f6b,_dl_fini,1

> [!IMPORTANT]  
> This version of the tool does not miss the `atoi`, `sqrt` and `printf` dynamic calls.

## A possible alternative

An out-of-the-box alternative to the library signature extraction is to use the `ltrace` tool available in Linux systems.
Consider the case for the bubblesort program.

    $ ltrace -b -o bubblesort.signature -- ./bubblesort 15 4 8 16 42 23
    4 8 15 16 23 42 
    $ cat bubblesort.signature 
    malloc(24)                                                                   = 0x558e0d9032a0
    atoi(0x7ffdda6c169f, 0, 0x558e0d9032a0, 0)                                   = 15
    atoi(0x7ffdda6c16a2, 5, 0x558e0d9032a0, 4)                                   = 4
    atoi(0x7ffdda6c16a4, 4, 0x558e0d9032a0, 8)                                   = 8
    atoi(0x7ffdda6c16a6, 8, 0x558e0d9032a0, 12)                                  = 16
    atoi(0x7ffdda6c16a9, 6, 0x558e0d9032a0, 16)                                  = 42
    atoi(0x7ffdda6c16ac, 2, 0x558e0d9032a0, 20)                                  = 23
    printf("%d ", 4)                                                             = 2
    printf("%d ", 8)                                                             = 2
    printf("%d ", 15)                                                            = 3
    printf("%d ", 16)                                                            = 3
    printf("%d ", 23)                                                            = 3
    printf("%d ", 42)                                                            = 3
    putchar(10, 0x558deb4a1006, 0, 0)                                            = 10
    free(0x558e0d9032a0)                                                         = <void>

It has a richer output, since it can track arguments and return values.
However, it suffers from the same limitations as rtld-audit, as it only works in binaries with PLT.
For instance, it fails to extract the library calls for the squareroot program.

    $ ltrace -b -o squareroot.signature -- ./squareroot 16
    √16 = 4
    $ cat squareroot.signature 
    +++ exited (status 0) +++

Thus, `ltrace` is not a suitable replacement for LibSIG, since it is not as fast as the rtld-audit version, nor as precise as the Valgrind's plugin.
