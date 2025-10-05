#include <stdlib.h>
#include <stdio.h>

// for stat
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "floppy.h"
#include "track.h"
#include "sector.h"
#include "bigendian.h"

// private functions
int num_sector_for_track(t_floppy *floppy,int t);
int get_floppy_size(t_floppy *floppy);
t_dir_entry *find_file(t_floppy *floppy, char *filename) ;

/**
 * @brief allocate memory for a t_floppy variable. num_track, track0_sectors &
 *        tracks_sectors must have a value
 *
 * @param floppy
 */
void floppy_allocate(t_floppy *floppy) {

    floppy->tracks = (t_track *)malloc(floppy->num_track * sizeof(t_track));
    if (floppy->tracks == NULL) {
        exit(EXIT_FAILURE);
    }

    for(int t=0;t<floppy->num_track;t++) {
        track_init_sectors(&floppy->tracks[t],num_sector_for_track(floppy,t));
    }

}

/**
 * @brief free memory for a t_floppy variable
 *
 * @param floppy
 */
void floppy_release(t_floppy *floppy) {

    t_track *track=floppy->tracks;
    for (int t=0;t<floppy->num_track;t ++) {
        free(track->sectors);
        track++;
    }

    free(floppy->tracks);
}

void floppy_build(t_floppy *floppy,int num_track,enum e_side side,enum e_density density) {

    floppy->num_track = num_track;
    floppy->side = side;
    floppy->density = density;
    floppy->track0_sectors = TRACK0_SECTORS;
    floppy->tracks_sectors = density==SINGLE_DENSITY?SD_SECTORS:DD_SECTORS;

    floppy_allocate(floppy);

}

/**
 * @brief create an empty disk structure
 *
 * @param floppy
 * @param label , max 10 characters
 * @param number , disk id between 0 and 65536
 */
void floppy_format(t_floppy *floppy,char *label,int number) {

    t_track *track;
    t_sector *sector;

    // track 0 = system data
    track = floppy->tracks;
    sector = track->sectors;

    // track 0 / sector 1 = boot sector
    empty_sector(sector++);

    // track 0 / sector 2 = 7 sector (2)
    empty_sector(sector++);

    // track 0 / sector 3 = System Information Record
    empty_sector(sector);

    for(int i=0;i<VOLUME_LABEL_MAXLENGTH;i++) {
        if (label[i]==0) break;
        sector->sir.volume_label[i] = label[i];
    }

    // get current date
    time_t timestamp = time(NULL);
    struct tm *timeinfo = localtime(&timestamp);

    bigendian_set(&sector->sir.volume_number,number);
    sector->sir.first_user_track=1;
    sector->sir.first_user_sector=1;
    sector->sir.last_user_track=floppy->num_track-1;
    sector->sir.last_user_sector=num_sector_for_track(floppy,1);
    bigendian_set(&sector->sir.total_sector, sector->sir.last_user_track * sector->sir.last_user_sector );
    sector->sir.creation_month = timeinfo->tm_mon+1;
    sector->sir.creation_day = timeinfo->tm_mday;
    sector->sir.creation_year = timeinfo->tm_year % 100;
    sector->sir.max_track = sector->sir.last_user_track;
    sector->sir.max_sector = sector->sir.last_user_sector;

    sector++;

    // track 0 / sector 4 = empty
    empty_sector(sector++);

    // track 0 / sector 5... = directory
    for (int s=4;s<num_sector_for_track(floppy,0);s++) {
        unsigned char next_sector=s+2;
        if ( s==(num_sector_for_track(floppy,0)-1) ) { next_sector=0; }
        empty_sector(sector);
        sector->dir.next_sector = next_sector;
        sector++;
    }

    // track 1... = user data
    for (int t=1;t<floppy->num_track;t ++) {
        track++;
        for(int s=0;s<num_sector_for_track(floppy,t);s ++) {

            t_sector *sector = &track->sectors[s];

            unsigned char next_track=t;
            unsigned char next_sector=s+2;

            if (s==(num_sector_for_track(floppy,t)-1)) {
                next_track++;
                next_sector=1;
                if (t==(floppy->num_track-1)) {
                    next_track=0;
                    next_sector=0;
                }
            }


            sector->usr.next_track = next_track;
            sector->usr.next_sector = next_sector;
        }
    }



}

/**
 * @brief export a .dsk file
 *
 * @param floppy
 * @param filename
 */
void floppy_export(t_floppy *floppy,char *filename) {
    int t = 0;

    FILE *fp = fopen(filename,"wb");
    if (fp == NULL) {
        perror("cannot open file");
        exit(EXIT_FAILURE);
    }

    if(floppy->squale_rom) { // track 0 is special: only 3 sectors are used
        t_track *track = &floppy->tracks[t++];
        fwrite(&track->sectors[0],SECTOR_SIZE,1,fp);
        fwrite(&track->sectors[2],SECTOR_SIZE,1,fp);
	for(int i=4; i<floppy->track0_sectors; ++i)
        fwrite(&track->sectors[i],SECTOR_SIZE,1,fp);
    }

    // TODO : track 0 must have the same size as other tracks
    for (;t<floppy->num_track;t ++) {
        t_track *track = &floppy->tracks[t];
        fwrite(track->sectors,SECTOR_SIZE,track->num_sector,fp);

        // special case if track 0 has different size : add some empty sectors
        // so that all tracks have the same length in the .dsk file
        if ( (t==0) && (floppy->track0_sectors < floppy->tracks_sectors) ) {
            for(int i=0; i<SECTOR_SIZE*(floppy->tracks_sectors - floppy->track0_sectors) ; i ++ )
                fputc(0,fp);
        }

    }

    fclose(fp);

}

static int allSame(uint8_t val, uint8_t *array, size_t len) {
    while(len--) if(array[len]!=val) return 0;
    return 1;
}	

/**
 * @brief find disk geometry
 *
 * @param floppy
 * @param filename
 * @return int
 */
int floppy_guess_geometry(t_floppy *floppy,char *filename) {

    struct stat sb;

    if (stat(filename,&sb)==-1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    int filesize = sb.st_size;

    // .dsk file should be a multiple of SECTOR_SIZE
    if (filesize%SECTOR_SIZE) {
        fprintf(stderr,"%s filesize (%d) is not a multiple of %d !\n",
            filename, filesize, SECTOR_SIZE);
        return 0;
    }

    int count_sectors = filesize/256;

    // at least 10 sectors
    if (count_sectors<10) {
        fprintf(stderr,"%s file is too small !\n",
            filename);
        return 0;
    }

    FILE *fp = fopen(filename,"rb");
    if (fp == NULL) {
        perror("cannot open file");
        exit(EXIT_FAILURE);
    }

    t_sector sector;
    // skip boot
    int num_read = fread(&sector,SECTOR_SIZE,1,fp);
    if (num_read!=1) {
        fprintf(stderr,"%s: Can't read boot block.\n",filename);
        exit(-2);
    }

    // track2 can be SIR for squale rom-pack
    num_read = fread(&sector,SECTOR_SIZE,1,fp);
    if (num_read!=1) {
        fprintf(stderr,"%s: Can't read sector 2.\n",filename);
        exit(-2);
    }
    // NdSam: squale rom-pack have SIR at sector 2 and some sectors missing
    // to fit the 64kb rom-pack.
    //
    // 64k rom theorical layout: 26 tracks 10 sectors/track
    // except t0 = 8 sectors, two of wich are skipped
    // total = 25*10 + 8 - 2 = 256 sectors --> 64kb
    //
    // t0s1 : boot
    // t0s2 : --- missing
    // t0s3 : sir
    // t0s4 : --- missing
    // t0s5 : dir
    // t0s6 : dir
    // t0s7 : dir
    // t0s8 : dir
    //
    // 32k rom theorical layout: 13 tracks 10 sectors/track
    // except t0 = 10 sectors, two of wich are skipped
    // total = 12*10 + 10 - 2 = 128 sectors --> 32kb
    //
    // t0s1 : boot
    // t0s2 : --- missing
    // t0s3 : sir
    // t0s4 : --- missing
    // t0s5 : dir
    // t0s6 : dir
    // t0s7 : dir
    // t0s8 : dir
    // t0s9 : dir
    // t0s10: dir
    //
    // 16k squale rom theorical layout: 7 tracks 10 sectors/track
    // exce0pt t0 = 6 sectors, two of wich are skipped
    // total = 7*10 + 6 - 2 = 64 sectors --> 16kb
    //
    // t0s1 : boot
    // t0s2 : --- missing
    // t0s3 : sir (should be t0s3)
    // t0s4 : --- missing
    // t0s5 : dir
    // t0s6 : dir
    if (filesize==(filesize&-filesize)
    &&  10==sector.sir.max_sector
    && sector.sir.last_user_track==sector.sir.max_track
    && sector.sir.last_user_sector==sector.sir.max_sector) {
        // squale rom-pack
        floppy->squale_rom = 1;
        floppy->num_track = sector.sir.max_track+1;
        floppy->tracks_sectors = sector.sir.max_sector;
    } else {
        // standard floppy : SIR
        floppy->squale_rom = 0;
        num_read = fread(&sector,SECTOR_SIZE,1,fp);
        if (num_read!=1) {
            fprintf(stderr,"%s: Can't read sectror 3 (SIR).\n",filename);
            exit(-2);
        }
        floppy->num_track = sector.sir.max_track+1;
        floppy->tracks_sectors = sector.sir.max_sector;
        // skip track 4
        num_read = fread(&sector,SECTOR_SIZE,1,fp);
        if (num_read!=1) {
            fprintf(stderr,"%s: Can't read sector 4 !\n",filename);
            exit(-2);
        }
    }

    // look for first user-sector and guess sector0 length from chaining
    int num_sector0 = 5, dir_sector = 5;
    while(!feof(fp)) {
        num_read = fread(&sector,SECTOR_SIZE,1,fp);
        if (num_read!=1) {
            fprintf(stderr,"%s: Can't read sector %d (DIR).\n",filename, dir_sector);
            exit(-2);
        }
	// dieqr
        if(sector.dir.next_sector==2 && sector.dir.next_track==1) break;
	if(sector.dir.next_sector==0 && sector.dir.next_track==0 && 
	   (sector.usr.sequence.digit[0] || sector.usr.sequence.digit[1] || 
	   !allSame(0,sector.usr.data, SECTOR_USR_DATA_LENGTH))) break;
        if(sector.dir.next_sector > num_sector0) num_sector0 = sector.dir.next_sector;
        ++dir_sector;
    }

    floppy->track0_sectors = num_sector0;

    floppy->side=SINGLE_SIDE;
    if (num_sector0>TRACK0_SECTORS) {
        floppy->side=DOUBLE_SIDE;
    }

    floppy->density=SD_SECTORS;
    floppy->track0_aligned=0;

    if (num_sector0 != floppy->tracks_sectors) {
        floppy->density=DD_SECTORS;

        // check if the disk image has empty sectors so that all tracks
        // have the same length on file
        if(num_sector0 < dir_sector)
            floppy->track0_aligned=1;
    }

    fclose(fp);

    floppy_allocate(floppy);

    return 1;
}

/**
 * @brief read a .dsk file and put data into the t_floppy struct.
 *        the t_floppy struct memory must be already allocated.
 *
 * @param floppy
 * @param filename
 */
void floppy_import(t_floppy *floppy,char *filename) {

    struct stat sb;

    if (stat(filename,&sb)==-1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    int filesize = sb.st_size;

    if (filesize != get_floppy_size(floppy)) {
        //perror("bad size");
        //exit(EXIT_FAILURE);
        fprintf(stderr,"Warning, bad file size : %d (should be %d)\n",
            filesize, get_floppy_size(floppy) );
    }

    FILE *fp = fopen(filename,"rb");
    if (fp == NULL) {
        perror("cannot open file");
        exit(EXIT_FAILURE);
    }


    int t=0;

    if(floppy->squale_rom) {
        t_track *track = &floppy->tracks[t++];

        memset(track->sectors, 0, track->num_sector*SECTOR_SIZE);

        // track 0 is special: onlmy 3 sectors used
        if(1!=fread(&track->sectors[0],SECTOR_SIZE,1,fp))
                fprintf(stderr,"%s: can't read BOOT sector !\n",filename);

        if(0>fseek(fp, SECTOR_SIZE*1, SEEK_SET)) perror(filename);

        if(1!=fread(&track->sectors[2],SECTOR_SIZE,1,fp))
                fprintf(stderr,"%s: can't read SIR sector !\n",filename);

        if(0>fseek(fp, SECTOR_SIZE*2, SEEK_SET)) perror(filename);

	for(int i=4; i<floppy->track0_sectors; ++i) 
        if(1!=fread(&track->sectors[i],SECTOR_SIZE,1,fp))
                fprintf(stderr,"%s: can't read DIR sector %d\n",filename, i+1);	
    }

    for (;t<floppy->num_track;t ++) {
        t_track *track = &floppy->tracks[t];

        memset(track->sectors, 0, track->num_sector*SECTOR_SIZE);

        int num_read = fread(track->sectors,SECTOR_SIZE,track->num_sector,fp);
        if (num_read!=track->num_sector && !floppy->squale_rom && t!=floppy->num_track-1) {
            printf("track=%d, num_read=%d / %d \n",t,num_read, track->num_sector);
            fprintf(stderr,"%s: fatal error !\n",filename);
            exit(-2);
        }
        // special case if track 0 has less sectors than other tracks :
        // in some .dsk files all tracks have the same length, so we have to seek
        // the sector difference
        if (!floppy->track0_aligned) continue;

        if ( (t==0) && (floppy->track0_sectors < floppy->tracks_sectors) ) {
            int err = fseek(fp, SECTOR_SIZE*(floppy->tracks_sectors - floppy->track0_sectors), SEEK_CUR);
            if (err) {
                fprintf(stderr,"%s: Read error !\n",filename);
                exit(-2);
            }
        }

    }

    fclose(fp);


}

/**
 * @brief display on screen general infos about the disk
 *
 * @param floppy
 */
void floppy_info(t_floppy *floppy) {

    t_sector *sector = &floppy->tracks->sectors[2];

    printf("Volume label    : %s\n",sector->sir.volume_label);
    printf("Volume number   : %u\n",bigendian_get(&sector->sir.volume_number));
    printf("Creation date   : %d/%d/%d\n",sector->sir.creation_day,sector->sir.creation_month,sector->sir.creation_year);
    printf("Tracks          : %d\n",floppy->num_track);
    printf("Free sectors    : %u\n",bigendian_get(&sector->sir.total_sector));
    printf("Max track       : %d\n",sector->sir.max_track);
    printf("Max sector      : %d\n",sector->sir.max_sector);

    if (floppy->track0_sectors != floppy->tracks_sectors) {
        printf("Track 0 sectors : %d\n",floppy->track0_sectors);
    }

    printf("\n");

}

/**
 * @brief display on screen the content of the directory
 *
 * @param floppy
 */
void floppy_cat(t_floppy *floppy) {
    int boot_track  = floppy->tracks[0].sectors[0].raw.data[5];
    int boot_sector = floppy->tracks[0].sectors[0].raw.data[6];
    
    char filename[13];

    printf("FILENAME EXT  SECTORS\tDATE\t TRACK,SECTOR\n");
    printf("---------------------------------------------\n");

    t_sector *sector = &floppy->tracks->sectors[4];
    int num_sectors = 0;

    for(;;) {

        for(int i=0;i<DIR_ENTRY_PER_SECTOR;i ++) {

            t_dir_entry *dir = &sector->dir.dir[i];

            // deleted file
            if ((uint8_t)(dir->filename[0])==0xff) continue;

            // end of directory
            if (dir->filename[0]==0) break;

            dir_get_filename_pretty(dir,filename);
            unsigned int file_sectors = bigendian_get(&dir->total_sector);

            printf("%12s  %7d  %02d/%02d/%02d  %4s  %02d,%02d\n",
                    filename,
                    file_sectors,
                    dir->creation_day, dir->creation_month,dir->creation_year,
		    dir->start_track==boot_track && 
		    dir->start_sector==boot_sector ? "boot"
		                                   : "",
                    dir->start_track,dir->start_sector
                    );

            num_sectors += file_sectors;

        }

        if (!sector->dir.next_sector) break;
        sector = &floppy->tracks->sectors[sector->dir.next_sector - 1] ;
    }

    printf("---------------------------------------------\n");
    printf("USED SECTORS  %7d\n", num_sectors);
 }


/**
 * @brief extract data from a t_floppy struct and put the files into outdir directory
 *
 * @param floppy
 * @param outdir
 */
void floppy_extract(t_floppy *floppy, char *outdir) {

    char filename[13];
    char outfile[OUTFILE_LEN];

    t_sector *sector = &floppy->tracks->sectors[4];

    for(;;) {

        for(int i=0;i<DIR_ENTRY_PER_SECTOR;i ++) {
            t_dir_entry *dir = &sector->dir.dir[i];

            if ((unsigned char)(dir->filename[0])==0xFF) continue; // deleted file
            if (dir->filename[0]==0) break;

            if (dir->start_track<1) continue; // should not happen
                                              // happens on DYNSTAR.DSK

            dir_get_filename(dir,filename);


            strncpy(outfile,outdir,OUTFILE_LEN-15);
            strcat(outfile,"/");
            strcat(outfile,filename);


            t_sector *file_sector = &(floppy->tracks[dir->start_track].sectors[dir->start_sector-1]);

            printf("Extract %s ", outfile);

            FILE *fp;
            fp = fopen(outfile,"wb");

            unsigned char current_track = dir->start_track;
            unsigned char current_sector = dir->start_sector;

            int total_sector = 0;

            for(;;) {

                total_sector ++;
               fwrite(&file_sector->usr.data,252,1,fp);


               if ((file_sector->usr.next_track==0) && (file_sector->usr.next_sector==0)) break;

                current_track = file_sector->usr.next_track;
                current_sector = file_sector->usr.next_sector;
                file_sector = &(floppy->tracks[current_track].sectors[current_sector-1]);
            }

            fclose(fp);

            if (current_track != dir->end_track) {
                printf("Error , end track should be %d (was %d) !\n",dir->end_track,current_track);
                break;
            }

            if (current_sector != dir->end_sector) {
                printf("Error , end sector should be %d (was %d) !\n",dir->end_sector,current_sector);
                break;
            }

            int dir_total_sector = bigendian_get(&dir->total_sector);
            if (total_sector != dir_total_sector) {
                printf("Error , total sector should be %d (was %d) !\n",dir_total_sector,total_sector);
                break;
            }

            printf(" \t- %d sectors.\n",total_sector);


        }

        if (!sector->dir.next_sector) break;
        sector = &floppy->tracks->sectors[sector->dir.next_sector - 1] ;
    }

}

/**
 * @brief add the file 'filename' from the host system to the t_floppy struct
 *
 * @param floppy
 * @param filename
 */
void floppy_add_file(t_floppy *floppy, char *filename) {

    t_sector *sir = &floppy->tracks->sectors[2];

    t_sector *sector = &floppy->tracks->sectors[4];
    t_dir_entry *dir;

    // get rid of path
    char *base_filename = strrchr(filename,'/');
    if (base_filename==NULL)
        base_filename=filename;
    else
        base_filename++;

    // check if the directory already exists
    dir = find_file(floppy,base_filename);
    if (dir) {
        fprintf(stderr,"%s already exists !\n", base_filename);
        exit(-1);
    }

    // find a free dir entry
    for(;;) {
        for(int i=0;i<DIR_ENTRY_PER_SECTOR;i ++) {
            dir = &sector->dir.dir[i];
            if ( ((unsigned char)(dir->filename[0])==0xFF) || (dir->filename[0]==0)) goto found_dir;
        }
        if (!sector->dir.next_sector) break;
        sector = &floppy->tracks->sectors[sector->dir.next_sector -1];
    }

    // no entry found
    fprintf(stderr,"No dir entry available on this floppy.\n");
    exit(-3);

    found_dir: ;

    //create dir entry
    int success = dir_set_filename(dir,base_filename);
    if (!success) {
        fprintf(stderr,"Cannot set filename %s\n",base_filename);
        exit(-3);
    }

    dir_set_current_date(dir);

    dir->random_file = 0;
    dir->start_track = sir->sir.first_user_track;
    dir->start_sector = sir->sir.first_user_sector;

    // add file content to sectors
    FILE *fp;
    fp = fopen(filename,"rb");
    if (!fp) {
        fprintf(stderr,"Cannot read %s\n",filename);
        exit(-3);
    }

    int num_sectors=0;
    int current_sector = dir->start_sector;
    int current_track = dir->start_track;
    sector = &floppy->tracks[current_track].sectors[current_sector-1];	

    while(!feof(fp)) {
	if(current_sector > floppy->tracks_sectors
	|| current_track  >= floppy->num_track) {
	      fprintf(stderr, "Disk full (%d,%d):(%d,%d)!\n",
	      current_track, current_sector,
	      floppy->num_track, floppy->tracks_sectors);
	      exit(-3);
	}

        num_sectors ++;
        bigendian_set(&sector->usr.sequence,num_sectors);

        memset(sector->usr.data,0,SECTOR_USR_DATA_LENGTH);
        int read_count = fread(sector->usr.data,1,SECTOR_USR_DATA_LENGTH,fp);
	//printf("%d -> %d\n", num_sectors, read_count);
        //if (read_count<SECTOR_USR_DATA_LENGTH) break; // file is 100% read
	if (feof(fp)) break;

        current_track = sector->usr.next_track;
        current_sector = sector->usr.next_sector;

        if (!current_track && !current_sector) {
            fprintf(stderr,"Disk full !\n");
            exit(-3);
        }

        sector = &floppy->tracks[current_track].sectors[current_sector-1];
    }

    fclose(fp);

    // update dir entry
    dir->end_track = current_track;
    dir->end_sector = current_sector;
    bigendian_set(&dir->total_sector,num_sectors);

    // update SIR free list with next available sector
    // TODO : check if this was the last available sector ?
    sir->sir.first_user_track = sector->usr.next_track;
    sir->sir.first_user_sector = sector->usr.next_sector;

    // update last file sector pointers
    sector->usr.next_track=0;
    sector->usr.next_sector=0;

    // update total sectors
    int total_sector = bigendian_get(&sir->sir.total_sector);
    total_sector -=num_sectors;
    bigendian_set(&sir->sir.total_sector,total_sector);

    char local_filename[13];
    dir_get_filename(dir,local_filename);
    printf("%s added to floppy. %d sectors free.\n",
        local_filename, total_sector);

}

/**
 * @brief set track into 6th byte of sector0/track0 and sector
 *        into 7th byte for file <filename>
 *
 * @param floppy
 * @param filename
 */

void floppy_set_boot(t_floppy *floppy, char *filename) {

    // find the file
    t_dir_entry *dir = find_file(floppy,filename);
    if (!dir) {
        fprintf(stderr,"Cannot find %s on floppy\n", filename);
        exit(-1);
    }

    t_sector *sector = floppy->tracks->sectors;
    sector->raw.data[5]=dir->start_track;
    sector->raw.data[6]=dir->start_sector;

    printf("Boot set to %s - %d/%d\n",
        filename,dir->start_track,dir->start_sector);

}

/****************************************************************************/
/* Private functions                                                        */
/****************************************************************************/

int num_sector_for_track(t_floppy *floppy,int t) {

        int num_sector = floppy->tracks_sectors;
        if (t==0) num_sector = floppy->track0_sectors;

        return num_sector;
}

// NOTE :   in some .dsk file format, if track 0 is smaller than other tracks, empty
//          sectors are added to track 0 so all tracks have the same size
//          ( example : DSTAR.DSK )
int get_floppy_size(t_floppy *floppy) {

    int size;

    if (floppy->track0_aligned)
        size = floppy->tracks_sectors*floppy->num_track;
    else
        size = floppy->track0_sectors + floppy->tracks_sectors*(floppy->num_track-1);

    if(floppy->squale_rom) { // round to lower power of two
        unsigned t = size;
        t |= t>>1;
        t |= t>>2;
        t |= t>>4;
        t |= t>>8;
        t |= t>>16;
        t ^= t>>1;
        size = t;
    }

    return size*SECTOR_SIZE;
}


t_dir_entry *find_file(t_floppy *floppy,char *filename) {

    t_sector *sector = &floppy->tracks->sectors[4];
    t_dir_entry *dir;
    char dir_filename[13];

    // browse dir entries
    for(;;) {
        for(int i=0;i<DIR_ENTRY_PER_SECTOR;i ++) {
            dir = &sector->dir.dir[i];
            if ( (unsigned char)(dir->filename[0])==0xFF) continue; // deleted file
            if ( dir->filename[0] == 0) return NULL; // end of directory, file not found

            dir_get_filename(dir,dir_filename);
            if (!strncmp(filename,dir_filename,13)) return dir; // file found

        }
        if (!sector->dir.next_sector) break;
        sector = &floppy->tracks->sectors[sector->dir.next_sector -1];
    }

    return NULL;
}
