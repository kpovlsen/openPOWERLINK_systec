  (c) SYSTEC electronic GmbH, D-07973 Greiz, August-Bebel-Str. 29
        www.systec-electronic.com
	openPOWERLINK.sourceforge.net


	openPOWERLINK - FPGA design for SYS TEC ECUcore-EP3C with openMAC
	==================================================================


Contents
---------

- FPGA design with Nios II CPU and POWERLINK IP core (includes openMAC)


Requirements
-------------

- Development Board for SYS TEC ECUcore-EP3C

- Altera Quartus II v11.0 SP1 or newer (Web Edition is also possible)
  -> https://www.altera.com/download/software/quartus-ii-we

- Altera Nios II Embedded Design Suite Legacy Tools v11.0 SP1 or newer
  -> https://www.altera.com/download/software/nios-ii

- Experiences with this development environment are required


How to build the design (generate the SOF file)
------------------------------------------------

These steps are only necessary if you want to change the FPGA design.
Otherwise you can use the supplied SOF file and go directly to step 6.

1. Open the Quartus project file nios_openMac.qpf with Altera Quartus II.

2. Open the SOPC Builder via menu "Tools" -> "SOPC Builder".

3. Press the button "Generate" in the SOPC Builder to regenerate the Nios II system.

4. Close the SOPC Builder when the generation has finished.

5. Start the compilation in the Quartus II window via menu "Processing" -> "Start Compilation".

6. Use the design with the supplied demo projects in the openPOWERLINK
   subdirectory Examples\altera_nios2\no_os\gnu.

   Therefor, execute the following command in the "Nios II Command Shell"
   before calling create-this-app to set the SOPC_DIR.

    $ export SOPC_DIR=../../../SYSTEC_ECUcore-EP3C/design_nios2_directIO

   Please refer to the readme.txt in the subdirectory of the demo project for
   further information.
