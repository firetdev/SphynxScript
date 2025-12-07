# Examples
To run an example, you'll first need to compile the source code for the interpreter (g++ main.cpp).

Then, select your example script, and move it into the same directory as the compiled executable.
Rename the example to "script.snx", which is the file the interpreter looks for.

## Syntax: end vs bracket
Designing SphynxScript gave me the freedom to make a programming that can be whatever I want it to be. 
I've decided to turn that freedom into the language's main philosophy. 
Therefore, there are two options for syntax: "end" and "brackets" (they can be changed in the ExecutionEngine,
there's not yet any way to set them in your script). The default is "end". The two folders here are the same examples,
but different syntaxes.

## List of examples
clock_simulation.snx:  a SphynxScript implementation of my Clock Simulation repo

scope.snx:  a program to show off SphynxScript's scope management

functions.snx:  an example of SphynxScript functions