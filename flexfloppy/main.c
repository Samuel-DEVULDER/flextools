#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include "floppy.h"

t_floppy floppy;

int new_flag=0;
int rom_flag=0;
int cat_flag=0;
int extract_flag=0;
int add_flag=0;
int del_flag=0;
int ren_flag=0;
int bootsector_flag=0;
int setboot_flag=0;

char *infile=NULL,*outfile=NULL,*path=NULL, *filename=NULL;
char *oldname=NULL, *newname=NULL;
char floppy_label[12];
int floppy_number=0;
int num_tracks=0,num_sectors=10;

void usage() {
    printf("Usage:\n");
    printf("flexfloppy --in <disk.dsk> --cat\n"); 
    printf("flexfloppy --in <disk.dsk> --extract <path>\n");
    printf("flexfloppy --new --tracks <num_tracks> [--sectors <num_sectors>] [--label <label>] [--number <number>] [--rompack] --out <disk.dsk>\n");
    printf("flexfloppy --in <disk.dsk> --add <filename>\n");
    printf("flexfloppy --in <disk.dsk> --del <filename>\n");
    printf("flexfloppy --in <disk.dsk> --ren <oldname> --as <newname>\n");
    printf("flexfloppy --in <disk.dsk> --bootsector <filename>\n");
    printf("flexfloppy --in <disk.dsk> --setboot <filename>\n");
    exit(-1);
}

void do_cat(char *filename) {
    floppy_guess_geometry(&floppy,filename); 
    floppy_import(&floppy,filename); 
    floppy_info(&floppy);
    floppy_cat(&floppy);
    floppy_release(&floppy);
}

void do_extract(char *filename, char *path) {
    floppy_guess_geometry(&floppy,filename);
    floppy_import(&floppy,filename);
    floppy_info(&floppy);
    floppy_extract(&floppy,path);
    floppy_release(&floppy);
}

void do_new(char *filename, int tracks, int sectors, char *label, int number) {
    floppy.num_track = tracks;
    floppy.track0_sectors = sectors;
    floppy.tracks_sectors = sectors;
    floppy.side = SINGLE_SIDE; // don't care
    floppy.density = SINGLE_DENSITY; // don't care
    floppy_allocate(&floppy); 
    floppy_format(&floppy,label,number);
    floppy_export(&floppy,filename);
    floppy_release(&floppy);
    printf("New disk %s created\n",filename);
}

void do_newrom(char *filename, int tracks, int sectors, char *label, int number) {
    static unsigned char SQUALE_boot[SECTOR_SIZE] = {
        0x20, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0xc1, 0x00, 0x00, 0x00,
        0x10, 0xce, 0xc0, 0x7f, 0xfc, 0xc1, 0x05, 0xfd, 0xc3, 0x00, 0x10, 0x8e,
        0xc4, 0x00, 0x8d, 0x35, 0x81, 0x02, 0x27, 0x10, 0x81, 0x16, 0x26, 0xf6,
        0x8d, 0x2b, 0xb7, 0xc1, 0x08, 0x8d, 0x26, 0xb7, 0xc1, 0x09, 0x20, 0xea,
        0x8d, 0x1f, 0xb7, 0xc1, 0x0a, 0x8d, 0x1a, 0xb7, 0xc1, 0x0b, 0x8d, 0x15,
        0x1f, 0x89, 0x4d, 0x27, 0xd9, 0xbe, 0xc1, 0x0a, 0x34, 0x14, 0x8d, 0x09,
        0x35, 0x14, 0xa7, 0x80, 0x5a, 0x26, 0xf5, 0x20, 0xc9, 0x10, 0x8c, 0xc4,
        0x00, 0x26, 0x0f, 0x8e, 0xc3, 0x00, 0xec, 0x84, 0x27, 0x0b, 0x8d, 0x0e,
        0x26, 0x9e, 0x10, 0x8e, 0xc3, 0x04, 0xa6, 0xa0, 0x39, 0x6e, 0x9f, 0xc1,
        0x08, 0x01, 0x8d, 0x0a, 0x5f, 0xb6, 0xf0, 0x48, 0xa7, 0x80, 0x5a, 0x26,
        0xf8, 0x39, 0x34, 0x02, 0x4d, 0x26, 0x03, 0x54, 0x20, 0x0a, 0x34, 0x04,
        0xc6, 0x0a, 0x4a, 0x3d, 0xeb, 0xe0, 0xcb, 0x02, 0xf1, 0xc1, 0x6d, 0x2f,
        0x0d, 0x7f, 0xc1, 0x6d, 0x86, 0x3c, 0xb7, 0xf0, 0x47, 0x86, 0x34, 0xb7,
        0xf0, 0x47, 0xf1, 0xc1, 0x6d, 0x27, 0x13, 0xb6, 0xc1, 0x6d, 0xf7, 0xc1,
        0x6d, 0xb0, 0xc1, 0x6d, 0x5f, 0x7d, 0xf0, 0x48, 0x5a, 0x26, 0xfa, 0x4c,
        0x26, 0xf6, 0x5f, 0x35, 0x82, 0xc6, 0x0b, 0x17, 0x00, 0xb6, 0x30, 0xc8,
        0x1b, 0xbd, 0xcd, 0x39, 0x17, 0x00, 0xf4, 0x30, 0x8d, 0x01, 0x61, 0xc6,
        0x07, 0x17, 0x00, 0xa4, 0x5f, 0x30, 0xc8, 0x1d, 0xbd, 0xcd, 0x39, 0x7e,
        0xcd, 0x24, 0x4f, 0x5f, 0xed, 0xc8, 0x1f, 0xed, 0xc8, 0x19, 0x7e, 0xcd,
        0x24, 0xbd, 0xcd, 0x24, 0x30, 0x4d, 0xc6, 0x01, 0xbd, 0xcd, 0x39, 0x8d,
        0x6f, 0x8e, 0xc8, 0x44, 0xc6, 0x08, 0x8d, 0x6f, 0x86, 0x2e, 0xbd, 0xcd,
        0x18, 0xc6, 0x03, 0x8d
    };
	
    int sector0 = 5; // no more for SQUALE-rom
    unsigned int size = (sector0-2) + (tracks-1)*sectors;
    unsigned int t = size; // power of two just greater than size
    t |= t>>1; t |= t>>2; t |= t>>4; t |= t>>8; t |= t>>16; ++t;
    floppy.track0_aligned = 0;
    floppy.squale_rom = 1;
    floppy.num_track = tracks;
    floppy.track0_sectors = sector0;
    floppy.tracks_sectors = sectors;
    floppy.side = SINGLE_SIDE; // don't care
    floppy.density = SINGLE_DENSITY; // don't care
    floppy_allocate(&floppy); 
    floppy_format(&floppy,label,number);
    
    // copy rom boot
    memcpy(floppy.tracks->sectors, SQUALE_boot, SECTOR_SIZE);
    
    floppy_export(&floppy,filename);
    floppy_release(&floppy);
    printf("New rompack %s created (%u / %u bytes)\n",filename, size*256, t*256);
}

void do_add(char *infile,char *filename) {
    floppy_guess_geometry(&floppy,infile); 
    floppy_import(&floppy,infile); 
    floppy_add_file(&floppy,filename);
    floppy_export(&floppy,infile);
    floppy_release(&floppy);
}

void do_del(char *infile,char *filename) {
    floppy_guess_geometry(&floppy,infile); 
    floppy_import(&floppy,infile); 
    floppy_del_file(&floppy,filename);
    floppy_export(&floppy,infile);
    floppy_release(&floppy);
}

void do_ren(char *infile,char *oldname,char *newname) {
    floppy_guess_geometry(&floppy,infile); 
    floppy_import(&floppy,infile); 
    floppy_ren_file(&floppy,oldname,newname);
    floppy_export(&floppy,infile);
    floppy_release(&floppy);
}

void do_bootsector(char *infile,char *filename) {
    floppy_guess_geometry(&floppy,infile); 
    floppy_import(&floppy,infile); 
    sector_load(floppy.tracks->sectors,filename);
    floppy_export(&floppy,infile);
    floppy_release(&floppy);
    printf("Bootsector %s installed on %s\n",filename,infile);
}

void do_setboot(char *infile,char *filename) {
    floppy_guess_geometry(&floppy,infile); 
    floppy_import(&floppy,infile); 
    floppy_set_boot(&floppy,filename);
    floppy_export(&floppy,infile);
    floppy_release(&floppy);
}

int main(int argc, char *argv[]) {

    int c;

    while(1) {

        static struct option long_options[] = {
            {"cat", no_argument, 0, 'c'},
            {"new", no_argument, 0 ,'n'},
            {"rompack", no_argument, 0 ,'p'},
            {"in", required_argument,0,'i'},
            {"out", required_argument,0,'o'},
            {"extract", required_argument,0,'x'},
            {"tracks", required_argument,0,'t'},
            {"sectors", required_argument,0,'s'},
            {"label", required_argument,0,'l'},
            {"number", required_argument,0,'u'},
            {"add", required_argument,0,'a'},
            {"del", required_argument,0,'d'},
            {"ren", required_argument,0,'M'},
            {"as", required_argument,0,'V'},
            {"bootsector", required_argument,0,'b'},
            {"setboot", required_argument,0,'z'},
            {0,0,0,0}
        };

        int option_index=0;

        c = getopt_long (argc, argv, "cnpi:o:x:t:s:l:u:a:d:M:V:b:z:",
                       long_options, &option_index);

        if (c==-1) break;

        switch(c) {
            case 'c':
                cat_flag=1;
                break; 

            case 'n':
                new_flag=1;
                break;

            case 'p':
                rom_flag=1;
                break;

            case 'i':
                infile=optarg;
                break;

            case 'o':
                outfile=optarg;
                break;

            case 'x':
                path=optarg;
                extract_flag=1;
                break;

            case 't':
                num_tracks=atoi(optarg);
                break;

            case 's':
                num_sectors=atoi(optarg);
                break;

            case 'l':
                memset(floppy_label,'\0',11);
                strncpy(floppy_label,optarg,10);
                break;
            
            case 'u':
                floppy_number=atoi(optarg);
                break;

            case 'a':
                add_flag = 1;
                filename = optarg;
                break;

            case 'd':
                del_flag = 1;
                filename = optarg;
                break;

            case 'M':
                ren_flag = 1;
                oldname = optarg;
                break;

            case 'V':
                ren_flag = 1;
                newname = optarg;
                break;

            case 'b':
                bootsector_flag = 1;
                filename = optarg;
                break;

            case 'z':
                setboot_flag = 1;
                filename = optarg;
                break;

            default:
                usage();
        }

    }

    // EXTRACT
    if ( (infile!=NULL) && (path!=NULL) && extract_flag ) {
        do_extract(infile,path);
        return 0;
    }

    // NEW
    if ( (outfile !=NULL) && (num_tracks>0) && (num_sectors>0) && new_flag ) {
	if(rom_flag) do_newrom(outfile,num_tracks,num_sectors,
	                       floppy_label,floppy_number);
	else         do_new(outfile,num_tracks,num_sectors,
	                    floppy_label,floppy_number);
	if(cat_flag) do_cat(infile);
        return 0;
    }

    // ADD
    if ( (infile!=NULL) && (filename!=NULL) && add_flag) {
        do_add(infile,filename);
	if(cat_flag) do_cat(infile);
        return 0;
    }

    // DEL
    if ( (infile!=NULL) && (filename!=NULL) && del_flag) {
        do_del(infile,filename);
	if(cat_flag) do_cat(infile);
        return 0;
    }

    // REN
    if ( (infile!=NULL) && (oldname!=NULL) && (newname!=NULL) && ren_flag) {
        do_ren(infile,oldname,newname);
	if(cat_flag) do_cat(infile);
        return 0;
    }

    // BOOT SECTOR
    if ( (infile!=NULL) && (filename!=NULL) && bootsector_flag) {
        do_bootsector(infile,filename);
        return 0;
    }

    // SET BOOT
    if ( (infile!=NULL) && (filename!=NULL) && setboot_flag) {
        do_setboot(infile,filename);
	if(cat_flag) do_cat(infile);
        return 0;
    }

    // CAT
    if ( (infile!=NULL) && cat_flag) {
        do_cat(infile);
        return 0;
    }

    usage();
   
    return 1;
}
