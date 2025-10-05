#include "directory.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void dir_get_filename(t_dir_entry *dir,char *str) {

    memset(str,0,13);

    strncpy(str,dir->filename,8);
    strcat(str,".");
    strncat(str,dir->ext,3);

}

void dir_get_filename_pretty(t_dir_entry *dir,char *str) {

    memset(str,' ',12); str[12]='\0';

    memcpy(str,dir->filename,8);
    for(int i=0;i<8;i++) 
        if ( str[i] == '\0' ) str[i] = ' ';
    strncpy(str+9,dir->ext,3);

}

int dir_set_filename(t_dir_entry *dir, char *str) {
    
    // find dot separator
    char *dot = strchr(str,'.');
    if (dot==NULL) return 0;

    // filename should be max. 8 characters
    if ((dot-str-1)>8) return 0;

    // extension should be max. 3 characters
    if ((strlen(dot)-1)>3) return 0;

    // clear entries content
    memset(dir->filename,'\0',8);
    memset(dir->ext,'\0',3);

    // set entries
    char *src,*dst;
    for(src=str,dst=dir->filename;*src&&(*src!='.');src++,dst++) *dst=*src;
    for(src=dot+1,dst=dir->ext;*src;src++,dst++) *dst=*src;

    return 1;
}

void dir_set_current_date(t_dir_entry *dir) {
    
    // get current date
    time_t timestamp = time(NULL);
    struct tm *timeinfo = localtime(&timestamp);

    dir->creation_day = timeinfo->tm_mday;
    dir->creation_month = timeinfo->tm_mon+1;
    dir->creation_year = timeinfo->tm_year % 100;
}


void dir_info(t_dir_entry *dir) {

    char filename[13];

    dir_get_filename(dir,filename);

    printf("Filename : %s\n",filename);
    printf("Start track/sector : %d,%d\n",dir->start_track,dir->start_sector);
    printf("End track/sector : %d,%d\n",dir->end_track,dir->end_sector);
    printf("Total sectors : %d\n",bigendian_get(&dir->total_sector));
    printf("Random file : %d\n",dir->random_file);
    printf("Creation date : %d/%d/%d\n\n",dir->creation_day,dir->creation_month,dir->creation_year);

}