/*
 [PURPOSE]
	 convert ascii file from Bankefficiency code into an xml file

 [INPUT]
	 an ascii file provideed by BankEfficiency code
	 which name is "Trigger.dat"
 
 optional:
	 a prototype called BE_Proto.xml given by BankEfficiency code 
	 for non standalone jobs
 	
 [OUTPUT]
	 the equivalent xml file called "Trigger.xml"
 
 [USAGE]
	 BEAscii2Xml 
	 no optino or argument

 [AUTHOR]
 	Thomas Cokelaer April 2004

[Compilation]
        gcc BEAscii2Xml.c -o BEAscii2Xml  -I. -I/home/cokelaer/share/include -I../lalapps -I/software/geopptools/include -I../
	gcc BEAscii2Xml.c -o BEAscii2Xml -I/<include file>
*/

#include <stdio.h>
#include <stdlib.h>

#include <lal/LIGOLwXMLHeaders.h>
#include "BankEfficiency.h"

#define BEASCII2XML_INPUT1 "Trigger.dat"
#define BEASCII2XML_INPUT2 "BE_Proto.xml"
#define BEASCII2XML_OUTPUT "Trigger.xml"



#define BANKEFFICIENCY_PARAMS_INPUT \
"         %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %d %d %f %f %f %f %d %d %d"

void BEAscii2XmlHelp(void);


int
main (int argc, char **argv ) 
{
  UINT4 start = 0;
  ResultIn trigger;
  FILE *input1, *input2;
  FILE *output;
  char sbuf[512];
  
  if (argc>1){
  if (strcmp(argv[1], "--help")==0){ 
	  BEAscii2XmlHelp();
  }  
  if (strcmp(argv[1], "-h")==0){ 
	  BEAscii2XmlHelp();
  }  
  }
  /* Main program starts here */
  /* First we open the file containing the ascii results */
  fprintf(stderr,"opening the xml output data file -- ");
  output = fopen(BEASCII2XML_OUTPUT,"w");
  fprintf(stderr,"done\n");

  fprintf(stderr,"opening the xml input data file -- ");
  if  ( (input1  = fopen(BEASCII2XML_INPUT1,"r"))==NULL){
    fprintf(stderr,"error while opening input file %s\n",BEASCII2XML_INPUT1);
    exit(0);
  }
  fprintf(stderr,"done\n");

  fprintf(stderr,"opening the xml prototype (argument of BankEfficiency code) -- ");
  if  ( (input2  = fopen(BEASCII2XML_INPUT2,"r"))==NULL){
    fprintf(stderr,"error while opening input file %s\n",BEASCII2XML_INPUT2);
    fprintf(stderr,"the xml file will not contains parameters information\n");
    PRINT_LIGOLW_XML_HEADER(output);
    fprintf(stderr,"creating the header file -- done\n");
  }
  else 
    {
      /* read prototype and save in outputfile */
      fprintf(stderr,"parsing the prototype  -- ");
      while(fgets(sbuf,1024,input2) !=NULL)
	fputs(sbuf, output);
    fprintf(stderr," done\n");
      
    }

    fprintf(stderr,"Print the header of the xml file -- ");
    fprintf(output, "%s", LIGOLW_XML_BANKEFFICIENCY);
    fprintf(stderr,"done\n");
  /* read ascii input and save in xml format */
    fprintf(stderr,"reading the ascii file -- ");
    do  
    {
      fscanf(input1,BANKEFFICIENCY_PARAMS_INPUT,
	     &trigger.psi0_trigger,
	     &trigger.psi3_trigger,
	     &trigger.psi0_triggerC,
	     &trigger.psi3_triggerC,
	     &trigger.psi0_inject, 
	     &trigger.psi3_inject,
	     &trigger.fend_trigger, 
	     &trigger.fend_triggerC, 
	     &trigger.fend_inject,
	     &trigger.totalMass_trigger,
	     &trigger.eta_trigger,
	     &trigger.totalMass_triggerC,
	     &trigger.eta_triggerC,
	     &trigger.mass1_inject,
	     &trigger.mass2_inject,
	     &trigger.rho_final,
	     &trigger.phase,
	     &trigger.alpha,
	     &trigger.alpha_f, 
	     &trigger.layer,
	     &trigger.bin,
	     &trigger.rho_finalC,
	     &trigger.phaseC,
	     &trigger.alphaC,
	     &trigger.alpha_fC, 
	     &trigger.layerC,
	     &trigger.binC, &trigger.coaTime


	     );
      if (start==0){
	      start+=1;
      }
      else 
      {
	      fprintf(output,"\n");
      }
      fprintf(output, BANKEFFICIENCY_PARAMS_ROW,
	      trigger.psi0_trigger,
	      trigger.psi3_trigger,
	      trigger.psi0_triggerC,
	      trigger.psi3_triggerC,
	      trigger.psi0_inject, 
	      trigger.psi3_inject,
	      trigger.fend_trigger, 
	      trigger.fend_triggerC, 
	      trigger.fend_inject,
	      trigger.totalMass_trigger,
	      trigger.eta_trigger,
	      trigger.totalMass_triggerC,
	      trigger.eta_triggerC,
	      trigger.mass1_inject,
	      trigger.mass2_inject,
	      trigger.rho_final,
	      trigger.phase,
	      trigger.alpha,
	      trigger.alpha_f, 
	      trigger.layer,
	      trigger.bin,
	      trigger.rho_finalC,
	      trigger.phaseC,
	      trigger.alphaC,
	      trigger.alpha_fC, 
	      trigger.layerC,
	      trigger.binC, trigger.coaTime);
    }
   while(!feof(input1));

    fprintf(stderr,"done\n");
  PRINT_LIGOLW_XML_TABLE_FOOTER(output);
  PRINT_LIGOLW_XML_FOOTER(output);

  fclose(output);
    fprintf(stderr,"closing xml file\n");

  return 0;
}


void BEAscii2XmlHelp(void)
{
  fprintf(stderr, "BEAscii2Xml help:\n");
  fprintf(stderr, "=================\n");
  fprintf(stderr, "[PURPOSE]\n\t%s\n \t%s\n \t%s\n \t%s\n", 
		  "That code reads a file in ascii format generated by BankEfficiency",
		  "code and generate the appropriate xml files. It is used if one forget",
		  "to use the appropriate option within BankEfficiency code (--print-result-xml)",
		  "or when the bankEfficiency code has been used within the condor ",
		  "submit file. In that case indeed several ascii files have been ",
		  "generated but no xml files.\n");
  
  fprintf(stderr, "[INPUT/OUTPUT]\n\t%s\n \t%s\n \t%s\n \t%s %s %s %s\n", 
		  "That code do not need any input argument but needs an input file",
		  "in ascii format (the user has to be sure it is in the appropriate",
		  "format) and then it writes an xml file into an output file \n", 
		  "the input and output file name are ", BEASCII2XML_INPUT1, "and", BEASCII2XML_OUTPUT);

  exit(1);
  

}
