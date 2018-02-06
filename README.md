ROSE - Real Time Operating System Experiment

ROSE has a tiny preemptive kernel which supports

1. Dynamic task management
2. Kernel timers
3. Mutex
4. Semaphores
5. Events
6. Memory byte/block pool
7. Message queues
8. Interrupts

I have currently added support to test the kernel on a x86 environment using macro CONFIG_X86_SIMULATOR. In simulator mode it helps to easily fix kernel bugs.
Currently trying to add support for Raspberry Pi. Working on the Kernel API documentation.

Folder structure :

apps/ - The actual application using the kernel APIs

arch/ - Arch specific code

include/

kernel/ - Rose kernel files

kernel/include - Rose kernel include files

lib/

out/ - obj files for the build

scripts/ - Build scripts

testapps/ - Kernel test applications


X86 SIMULATOR:

To build a binary for X86 simulator run 'make -f scripts/Makefile.x86'. Add the application files in the apps/ folder and build the kernel. An exceutable named 'rosex86' will be created.
Also the simulator needs system ticks which can be generated using system_clk_100ms or system_clk_10ms executable files generated using scripts/send_signal.c. After running the kernel the tick executable should be run in another shell(in the same folder as 'rosex86')
