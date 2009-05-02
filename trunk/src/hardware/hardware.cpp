/*
 *  Copyright (C) 2002  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	This could do with a serious revision :)
*/

#include <string.h>
#include "dosbox.h"
#include "programs.h"
#include "hardware.h"

static HWBlock * firsthw=0;


class HWSET : public Program {
public:
	HWSET(PROGRAM_Info * program_info);
	void Run(void);

};


HWSET::HWSET(PROGRAM_Info * info):Program(info) {

}

void HW_Register(HWBlock * block) {
	block->next=firsthw;
	firsthw=block;
}


void HWSET::Run(void) {
	/* Hopefull enough space */
	char buf[1024];

	HWBlock * loopblock;
	if 	(!*prog_info->cmd_line) {
	/* No command line given give overview of hardware */
		loopblock=firsthw;
		WriteOut("旼컴컴컴컫컴컴컴컴컴컴컴컴컴컴컴컴컫컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴커");
		WriteOut("쿏evice  쿑ull Name                쿞tatus                                     �");
		WriteOut("쳐컴컴컴컵컴컴컴컴컴컴컴컴컴컴컴컴컵컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴캑");
		while (loopblock) {
			if (loopblock->show_status) {
				loopblock->show_status(buf);
			} else {
				strcpy(buf,"No Status Handler");
			}
			WriteOut("�%-8s�%-25s�%-43s�",loopblock->dev_name,loopblock->full_name,buf);
			loopblock=loopblock->next;
		}
		WriteOut("읕컴컴컴컨컴컴컴컴컴컴컴컴컴컴컴컴컨컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴켸");
		WriteOut("If you want to setup specific hardware use \"HWSET Device\" for information.\n");
		return;
	}
	/* Command line given */
	if (strcmp(prog_info->cmd_line,"/?")==0) {
		WriteOut("Hardware setup tool for DOSBox.\n");
		WriteOut("This program can be used to change the settings of internal hardware.\n\n"
			"HWSET [device] [switches]\n\n"
			"Using an empty command line gives you a list of hardware.\n"
			"You can get list of switches for a device with HWSET device.\n"
		);
		return;
	}
	char * rest=strchr(prog_info->cmd_line,' ');
	if (rest) *rest++=0;
	loopblock=firsthw;
	while (loopblock) {
		if (strcasecmp(loopblock->dev_name,prog_info->cmd_line)==0) goto founddev;
		loopblock=loopblock->next;	
	}
	WriteOut("Device %s not found\n",prog_info->cmd_line);
	return;
founddev:
/* Check for rest of line */
	if (rest) {
		strcpy(buf,rest);
		loopblock->get_input(buf);
		WriteOut(buf);

	} else {
		WriteOut("Command overview for %s\n%s",loopblock->full_name,loopblock->help);
	}
	return;
}

static void HWSET_ProgramStart(PROGRAM_Info * info) {
	HWSET * tempHWSET=new HWSET(info);
	tempHWSET->Run();
	delete tempHWSET;
}



void HARDWARE_Init(void) {
	PROGRAMS_MakeFile("HWSET.COM",HWSET_ProgramStart);

};

