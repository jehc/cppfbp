// #include "thxanch.h"
#include "thxiip.h"
#include "cppfbp.h"
#include "thxscan.h"
#include <stdio.h>
#include <stdarg.h>
#define MAXELEMNO 200
#define INPUT 0
#define OUTPUT 1
#define _CRT_SECURE_NO_WARNINGS

Process * find_proc(proc_ent *proc_tab, char name[32]);
int thz(int code, Process *proc_ptr, ...);

typedef int (CALLBACK* LPFNDLLFUNC)(_anchor);
LPFNDLLFUNC lpfnDllFunc;  

int thxbnet(label_ent * label_ptr, Process *mother,
	Appl *appl_ptr, label_ent *label_tab) {

		/*

		thxbnet processes one label only, generating the control structures required to run the application
		based on the fixed format network definitions

		*/

		cp *cpp, *cpp2;
		Process *this_proc;
		proc_ent *curr_proc;

		Cnxt *cnxt_ptr;
		Process *upstream_proc, *downstream_proc;
		cnxt_ent *curr_cnxt;
		label_ent *leptr;
		int i;

		char szBuf[80];
		HINSTANCE hDLL;
		char procname_in_dll[255];
		char dllname[255];

		curr_proc = label_ptr -> proc_ptr;

		while (curr_proc != 0) {
			//this_proc = (Process *) malloc(sizeof(Process));
			this_proc = new Process();
			strcpy(this_proc -> procname, curr_proc -> proc_name);
			strcpy(this_proc -> compname, curr_proc -> comp_name);

			this_proc -> appl_ptr = appl_ptr;

			this_proc -> next_proc = 0;
			//this_proc -> has_run = FALSE;

			this_proc -> status = NOT_STARTED;
			//this_proc -> next_proc = 0;
			//this_proc -> first_owned_IP = 0;
			this_proc -> owned_IPs = 0;
			this_proc -> proc_anchor.svc_addr = (int (*) (int, void * vp, ...)) thz;
			this_proc -> proc_anchor.reserved = this_proc;
			this_proc -> mother_proc = mother;
			this_proc -> faddr = 0;
			this_proc -> in_cps = 0;
			this_proc -> out_cps = 0;
			this_proc -> begin_cp = 0;
			this_proc -> end_cp = 0;
			strcpy(this_proc -> int_pe.port_name, " ");
			this_proc -> int_pe.elem_count = 1;
			this_proc -> int_pe.cpptr = 0;
			this_proc -> int_pe.ret_code = 0;
			this_proc -> stack = 0;
			this_proc -> trace = curr_proc -> trace;
			this_proc -> composite = curr_proc -> composite;
			//this_proc -> terminating = FALSE;
			curr_proc -> proc_block = this_proc;
			curr_proc = curr_proc -> succ;
			
		}

		// now go through connection list - cnxt_ent's not Cnxt's
		curr_cnxt = label_ptr -> cnxt_ptr;

		while (curr_cnxt != 0) {

			if (curr_cnxt -> downstream_name[0] == '*') { // something to do with subnets
				cpp2 = mother -> out_cps;
				while (cpp2 != 0) {
					if (strcmp(curr_cnxt -> downstream_port_name, cpp2 -> port_name)
						== 0)
						break;
					cpp2 = cpp2 -> succ;
				}
				if (cpp2 == 0)
					cnxt_ptr = 0;
				else {
					cnxt_ptr = (Cnxt *) cpp2 ->
						elem_list[curr_cnxt -> downstream_elem_no].gen.connxn;
					cpp2 -> elem_list[curr_cnxt -> downstream_elem_no].subdef =
						TRUE;
				}

				goto build_out_cp;
			}

			//  not subnet port...
			downstream_proc = find_proc(label_ptr -> proc_ptr,
				curr_cnxt -> downstream_name);
			if (downstream_proc == 0)
				return(4);

			cpp = downstream_proc -> in_cps;

			while (cpp != 0) {
				if (strcmp(curr_cnxt -> downstream_port_name, cpp -> port_name) == 0)
					break;
				cpp = cpp -> succ;
			}
			i = curr_cnxt -> downstream_elem_no;
			if (i> MAXELEMNO) {
				printf ("Input port element no. exceeds maximum");
				return -1;
			}

			i++;

			if (cpp != 0) {  //  matching port name found 
				cpp -> elem_count = max(cpp -> elem_count, i);
			}
			else {

				// allocate block comprising a cp followed by MAXELEMNO cpelem's

				// this shouldn't happen more than once for a given port name... 

				cpp =  (cp *)malloc(sizeof(cp) + MAXELEMNO * sizeof(cp_elem));
				strcpy(cpp -> port_name, curr_cnxt -> downstream_port_name);
				cpp -> elem_count = i;
				cpp -> direction = INPUT;
				cpp -> succ = downstream_proc -> in_cps;
				downstream_proc -> in_cps = cpp;

				for (i = 0; i < MAXELEMNO; i++) {
					cpp -> elem_list[i].gen.connxn = 0;
					cpp -> elem_list[i].closed = FALSE;
					cpp -> elem_list[i].is_IIP = FALSE;
					cpp -> elem_list[i].subdef = FALSE;
				}
			}

			if (curr_cnxt -> downstream_port_name[0] == '*')
				downstream_proc -> begin_cp = cpp;

			i = curr_cnxt -> downstream_elem_no;

			if (curr_cnxt -> upstream_name[0] != '!') {

				if ((cnxt_ptr = (Cnxt *) cpp -> elem_list[i].gen.connxn) == 0) {
					if (cpp -> elem_list[i].is_IIP)
						printf("Cannot have connection and IIP on same port element\n");
					if (curr_cnxt -> upstream_name[0] == '*') {
						cpp2 = mother -> in_cps;
						while (cpp2 != 0) {
							if (strcmp(curr_cnxt -> upstream_port_name, cpp2 -> port_name) ==
								0)
								break;
							cpp2 = cpp2 -> succ;
						}
						if (cpp2 == 0)
							cnxt_ptr = 0;
						else
						{
							cnxt_ptr = (Cnxt *) cpp2 ->
								elem_list[curr_cnxt -> upstream_elem_no].gen.connxn;
							if (cpp2 -> elem_list[curr_cnxt -> upstream_elem_no].subdef)
								printf("Input port %s of subnet already defined\n",
								cpp2 -> port_name);
							else {
								if (cnxt_ptr != 0) {
									cpp2 -> elem_list[curr_cnxt -> upstream_elem_no].subdef =
										TRUE;
									if (cpp -> elem_list[i].gen.connxn != 0) {
										printf("Connection already in use\n");
										return(4);
									}
									else
										cpp -> elem_list[i].gen.connxn = cnxt_ptr;

									if (cpp2 -> elem_list[curr_cnxt -> upstream_elem_no].is_IIP)
										cpp -> elem_list[i].is_IIP = TRUE;
									else {
										cnxt_ptr -> fed_proc = downstream_proc;
										strcpy(cnxt_ptr -> name, cnxt_ptr -> fed_proc -> procname); 
										strcat(cnxt_ptr -> name, ".");
										strcat(cnxt_ptr -> name, cpp2 -> port_name);
										if (i > 0) {
											char no[12];
											sprintf(no, "[%d]", i); 
											strcat(cnxt_ptr -> name, no);
										}
									}
								}
							}
						}

						goto get_next_conn;
					}

					// now these are Cnxt's

					cnxt_ptr =  new Cnxt();				
					cnxt_ptr -> succ = (Cnxt *)appl_ptr -> first_cnxt;					
					appl_ptr -> first_cnxt = cnxt_ptr;

					cnxt_ptr -> first_IPptr = 0;
					cnxt_ptr -> last_IPptr = 0;
					cnxt_ptr -> IPcount = 0;
					cnxt_ptr -> max_IPcount = curr_cnxt -> capacity;
					if (cnxt_ptr -> max_IPcount == -1)
						cnxt_ptr -> max_IPcount = 1;
					//cnxt_ptr -> procs_wtg_to_send = 0;
					cnxt_ptr -> total_upstream_proc_count = 0;
					cnxt_ptr -> nonterm_upstream_proc_count = 0;
					cnxt_ptr -> closed = FALSE;
					if (cpp -> elem_list[i].gen.connxn != 0) {
						printf("Connection already in use\n");
						return(4);
					}
					else
						cpp -> elem_list[i].gen.connxn = cnxt_ptr;

					cnxt_ptr -> fed_proc = downstream_proc;
					strcpy(cnxt_ptr -> name, cnxt_ptr -> fed_proc -> procname); 
					strcat(cnxt_ptr -> name, ".");
					strcat(cnxt_ptr -> name, cpp -> port_name);
					if (i > 0) {
						char no[12];
						sprintf(no, "[%d]", i); 
						strcat(cnxt_ptr -> name, no);
					}
					//cnxt_ptr -> fedproc_wtg_to_recv = FALSE;
				}
build_out_cp:
				upstream_proc = find_proc(label_ptr -> proc_ptr,
					curr_cnxt -> upstream_name);

				cpp = upstream_proc -> out_cps;
				while (cpp != 0) {
					if (strcmp(curr_cnxt -> upstream_port_name, cpp -> port_name) == 0)
						break;
					cpp = cpp -> succ;
				}
				i = curr_cnxt -> upstream_elem_no;
				
			
			if (i> MAXELEMNO) {
				printf ("Output port element no. exceeds maximum");
				return -1;
			}

			i++;

			if (cpp != 0) {  //  matching port name found 
				cpp -> elem_count = max(cpp -> elem_count, i);
			}
			else {

				// allocate block comprising a cp followed by MAXELEMNO cpelem's

				// this shouldn't happen more than once for a given port name... 

				cpp =  (cp *)malloc(sizeof(cp) + MAXELEMNO * sizeof(cp_elem));
				strcpy(cpp -> port_name, curr_cnxt -> upstream_port_name);
				cpp -> elem_count = i;
				cpp -> direction = OUTPUT;
				cpp -> succ = upstream_proc -> out_cps;
				upstream_proc -> out_cps = cpp;

				for (i = 0; i < MAXELEMNO; i++) {
					cpp -> elem_list[i].gen.connxn = 0;
					cpp -> elem_list[i].closed = FALSE;
					cpp -> elem_list[i].is_IIP = FALSE;
					cpp -> elem_list[i].subdef = FALSE;
				}
			}
			


				if (curr_cnxt -> upstream_port_name[0] == '*')
					upstream_proc -> end_cp = cpp;

				i = curr_cnxt -> upstream_elem_no;

				curr_cnxt -> gen.connxn = cnxt_ptr;
			}
			else
				cpp -> elem_list[i].is_IIP = TRUE;
			if (cpp -> elem_list[i].gen.connxn != 0) {
				printf("Connection already in use\n");
				return(4);
			}
			// copy in connection or IIP address to element
			cpp -> elem_list[i].gen.connxn = (Cnxt*) curr_cnxt -> gen.connxn;

get_next_conn:  curr_cnxt = curr_cnxt -> succ;
		}


		curr_proc = label_ptr -> proc_ptr;

		while (curr_proc != 0) {
			this_proc = (Process *) curr_proc -> proc_block;
			if (curr_proc -> composite) {

				leptr = (label_ent *) curr_proc -> label_ptr;

				/* this will cause recursive use of thxbnet */
				thxbnet(leptr,
					this_proc,
					appl_ptr,
					label_tab);

				cpp = this_proc -> out_cps;
				while (cpp != 0)  {
					for (i = 0; i < cpp -> elem_count; i++) {
						cnxt_ptr = (Cnxt *) cpp -> elem_list[i].gen.connxn;
						if (cnxt_ptr == 0)
							continue;
						if (!cpp -> elem_list[i].subdef)
							printf("Port %s used by process %s but not defined\n",
							cpp -> port_name, this_proc -> procname);
						cpp -> elem_list[i].gen.connxn = 0;
					}
					cpp = cpp -> succ;
				}

				cpp = this_proc -> in_cps;
				while (cpp != 0)
				{
					for (i = 0; i < cpp -> elem_count; i++) {
						cnxt_ptr = (Cnxt *) cpp -> elem_list[i].gen.connxn;
						if (cnxt_ptr == 0)
							continue;
						if (!cpp -> elem_list[i].subdef)
							printf("Port %s used by process %s but not defined\n",
							cpp -> port_name, this_proc -> procname);
						cpp -> elem_list[i].gen.connxn = 0;
					}
					cpp = cpp -> succ;
				}

				this_proc -> next_proc = (Process*) appl_ptr -> first_child_proc;
				appl_ptr -> first_child_comp = this_proc;
			}
			else {
				if (curr_proc -> faddr == 0) {

					/* The following assumes that components are packaged one component per DLL - 
					of course they could be packaged in many other ways */

					SetErrorMode(SEM_NOOPENFILEERRORBOX);

					if (curr_proc -> comp_name[0] == '\0') {
						strcpy(szBuf, "Component name not filled in");
						MessageBox(NULL, szBuf, "Library Functions", MB_ICONHAND);
					}

					/* Load the library module. This assumes one DLL per component */
					//strcpy(dllname, curr_proc -> comp_name);

					//strcpy(dllname, "../Debug/");
					//strcat(dllname, curr_proc -> comp_name);
					//strcat(dllname, "Dll.dll");
					strcpy(dllname, "CppFBPComponents.dll");
					hDLL = LoadLibrary(dllname);
					if (hDLL == NULL) {
						strcpy(szBuf, "LoadLibrary failed: ");
						strcat(szBuf, dllname);
						MessageBox(NULL, szBuf, "Library Functions", MB_ICONHAND);
					}
					else {

						/* Retrieve the address of the actual function. */
						//lpfnDllFunc = (LPFNDLLFUNC1)
						// (int (FAR PASCAL *)(anchor anch))
						//	(GetProcAddress(hDLL, procname_in_dll));
						strcpy(procname_in_dll, "_");
					    strcat(procname_in_dll, curr_proc -> comp_name);
					    strcat(procname_in_dll, "@8");	
						lpfnDllFunc = (LPFNDLLFUNC)	GetProcAddress(hDLL, procname_in_dll);
						curr_proc -> faddr  = lpfnDllFunc;

						if (curr_proc -> faddr == NULL) {
							GetLastError();
							strcpy(szBuf, "GetProcAddress failed: ");
							strcat(szBuf, procname_in_dll);
							MessageBox(NULL, szBuf, "Library Functions", MB_ICONHAND);
						}

					}
				}




				this_proc -> faddr =  curr_proc -> faddr;
				//this_proc -> must_run = curr_proc -> must_run;
				cpp = this_proc -> out_cps;
				while (cpp != 0)  {
					for (i = 0; i < cpp -> elem_count; i++) {
						cnxt_ptr = (Cnxt *) cpp -> elem_list[i].gen.connxn;
						if (cnxt_ptr == 0)
							continue;
						cnxt_ptr -> total_upstream_proc_count++;
						cnxt_ptr -> nonterm_upstream_proc_count++;
					}
					cpp = cpp -> succ;
				}

				this_proc -> next_proc = (Process*) appl_ptr -> first_child_proc;
				appl_ptr -> first_child_proc = this_proc;
			}

			curr_proc = curr_proc -> succ;
		}
		return -1;
}

Process * find_proc(proc_ent *proc_tab, char name[32])
{
	proc_ent *PEptr;
	PEptr = proc_tab;
	while (PEptr != 0) {
		if (strcmp(PEptr -> proc_name, name) == 0)
			break;
		PEptr = PEptr -> succ;
	}
	if (PEptr == 0) {
		printf ("Process name %s not found\n", name);
		return(0);
	}
	return (Process *)(PEptr -> proc_block);
}