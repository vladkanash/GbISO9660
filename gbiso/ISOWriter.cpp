//
// Created by vladkanash on 10/22/15.
//
#include "ISOWriter.h"

#define SYSTEM_ID "LINUX"
#define VOLUME_ID "MY_ISO_IMAGE"
#define PUBLISHER "ISOWriter v.1.0"
#define APP_ID "ISOWriter v.1.0"

#define VOL_COUNT 1
#define VOL_INDEX 1

#define PATH_TABLE_LOCATION_LSB 0x13
#define PATH_TABLE_LOCATION_MSB 0x15
#define ROOT_ENTRY_LOCATION 0x17
#define FIRST_DIR_ENTRY_LOCATION 0x19
#define ROOT_ENTRY_SIZE 0x0A
#define NAME_FILLER '_'

using namespace std;

ISOWriter::ISOWriter(char **files, int filecount)
{
    total_block_size = 0;
    dir_count = 0;
    fstream = 0;
    file_tree_depth = 0;

    printf("%s\n", "Building file tree...");
    printf("%s\n", "Files:");
    int res = create_file_tree(files, filecount);

    if (res == -1)
    {
        printf("%s\n", "Error building file tree");
        exit(-1);
    }
}

ISOWriter::~ISOWriter()
{
    destroy_tree_recursive(file_tree);
    delete file_tree;
}

int ISOWriter::write_iso(const char *filename)
{
    struct timeval t1, t2;
    double elapsedTime;

    // start timer
    gettimeofday(&t1, NULL);
    fstream = fopen(filename, "wb");
    if(!fstream)
        return -1;

    printf("Creating file...");
    create_blank_file();
    printf("Done\nSetting image descriptors...");

    set_volume_descriptors();
    printf("Done\nCreating path tables...");

    set_path_table();
    printf("Done\nCopying data...");

    set_dir_entries();  //TODO modification time
    printf("Done\n");

    fclose(fstream);
    //end timer
    gettimeofday(&t2, NULL);

    elapsedTime = t2.tv_sec - t1.tv_sec;
    elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000000.0;   // us to s

    printf("Your ISO image %s succesfully created in %.3f sec\n",
           filename,
           elapsedTime);
    return 0;
}

void ISOWriter::set_path_table() {
    set_root_path_entry(LITTLE_END);
    set_root_path_entry(BIG_END);
    global_offset = PATH_TABLE_LOCATION_LSB * SECTOR_SIZE + ROOT_ENTRY_SIZE;
    set_path_table_content(LITTLE_END, file_tree);
    global_offset = PATH_TABLE_LOCATION_MSB * SECTOR_SIZE + ROOT_ENTRY_SIZE;
    set_path_table_content(BIG_END, file_tree);
}

void ISOWriter::set_volume_descriptors() {
    volume_descriptor tvd;
    primary_volume_descriptor pvd;

    set_pvd(&pvd);
    set_tvd(&tvd);

    fseek(fstream, VOLUME_DESCRIPTOR_START_SECTOR * SECTOR_SIZE, SEEK_SET);

    fwrite(&pvd, SECTOR_SIZE, 1, fstream);
    fwrite(&tvd, SECTOR_SIZE, 1, fstream);
}

void ISOWriter::create_blank_file() const {
    char buffer[SECTOR_SIZE];
    memset(buffer, 0, SECTOR_SIZE);

    long volume_size = total_block_size + FIRST_DIR_ENTRY_LOCATION + dir_count * 2;

    for (int i = 0; i < volume_size; i++)
    {
        fwrite(&buffer, SECTOR_SIZE, 1, fstream);
    }
}

void ISOWriter::set_pvd(primary_volume_descriptor *pDescriptor) {

    memset(pDescriptor, 0, sizeof(primary_volume_descriptor));

    set_value((char*)pDescriptor->magic, VOLUME_DESCRIPTOR_SIGNATURE,
              sizeof(pDescriptor->magic),
              sizeof(VOLUME_DESCRIPTOR_SIGNATURE));

    set_value((char*)pDescriptor->sys_id, SYSTEM_ID,
              sizeof(pDescriptor->sys_id),
              sizeof(SYSTEM_ID), ' ');

    set_value((char*)pDescriptor->vol_id, VOLUME_ID,
              sizeof(pDescriptor->vol_id),
              sizeof(VOLUME_ID), ' ');

    set_value((char*)pDescriptor->vol_set_id, VOLUME_ID,
              sizeof(pDescriptor->vol_set_id),
              sizeof(VOLUME_ID), ' ');

    set_value((char*)pDescriptor->app_id, APP_ID,
              sizeof(pDescriptor->app_id),
              sizeof(APP_ID), ' ');

    set_value((char*)pDescriptor->publisher_id, PUBLISHER,
              sizeof(pDescriptor->publisher_id),
              sizeof(PUBLISHER), ' ');

    set_value((char*)pDescriptor->data_preparer_id, PUBLISHER,
              sizeof(pDescriptor->data_preparer_id),
              sizeof(PUBLISHER), ' ');

    pDescriptor->type = PRIMARY_VOLUME_DESCRIPTOR;
    pDescriptor->version = 1;
    pDescriptor->file_structure_version = 1;

    memset(pDescriptor->copyright_file, ' ', sizeof(pDescriptor->copyright_file));
    memset(pDescriptor->abstract_file, ' ', sizeof(pDescriptor->abstract_file));
    memset(pDescriptor->biblio_file, ' ', sizeof(pDescriptor->biblio_file));

    set_pvd_time(pDescriptor);

    pDescriptor->vol_count = get_both_endian((u16)VOL_COUNT);
    pDescriptor->vol_index = get_both_endian((u16)VOL_COUNT);
    pDescriptor->logical_block_size = get_both_endian((u16)SECTOR_SIZE);

    pDescriptor->path_table_size = get_both_endian((u32)path_table_size);
    unsigned volume_size = total_block_size + FIRST_DIR_ENTRY_LOCATION + dir_count * 2;
    pDescriptor->vol_size = get_both_endian((u32)volume_size);

    pDescriptor->path_table_location_LSB = PATH_TABLE_LOCATION_LSB;
    pDescriptor->path_table_location_MSB = big_endian((u32)PATH_TABLE_LOCATION_MSB);

    directory_record root_record;
    memset(&root_record, 0, sizeof(directory_record));
    root_record = get_dir_record(*file_tree);

    memcpy(pDescriptor->root_entry, &root_record, sizeof(root_record));
}

both_endian_16 ISOWriter::get_both_endian(u16 value) {
    both_endian_16 buffer;
    buffer.little = value;
    buffer.big = big_endian(value);
    return buffer;
}


both_endian_32 ISOWriter::get_both_endian(u32 value) {
    both_endian_32 buffer;
    buffer.little = value;
    buffer.big = big_endian(value);
    return buffer;
}


void ISOWriter::set_value( char *field,
                           const char* value,
                           const size_t field_size,
                           const size_t value_size,
                           char filler)
{
    memset(field, filler, field_size);
    strcpy(field, value);
    field[value_size - 1] = filler;
}

void ISOWriter::set_pvd_time(primary_volume_descriptor *pDescriptor)
{
    time_t cur_time;
    time(&cur_time);
    struct tm* now = localtime( &cur_time );

    struct s_date_time pvd_time;
    memset(&pvd_time, 0, sizeof(pvd_time));

    itoa((char*)pvd_time.year, now->tm_year + 1900);
    itoa((char*)pvd_time.month, now->tm_mon + 1);
    itoa((char*)pvd_time.day,  now->tm_mday);
    itoa((char*)pvd_time.hour, now->tm_hour);
    itoa((char*)pvd_time.minute, now->tm_min);
    itoa((char*)pvd_time.second, now->tm_sec);

    pDescriptor->vol_creation = pvd_time;
    pDescriptor->vol_modif = pvd_time;
}

void ISOWriter::itoa(char *dest, const int src) const {

    char y_buffer[4];
    sprintf(y_buffer, "%d", src);
    strcpy (dest, y_buffer);
}

void ISOWriter::set_tvd(volume_descriptor *pDescriptor)
{
    memset(pDescriptor, 0, sizeof(volume_descriptor));

    set_value((char*)pDescriptor->magic, VOLUME_DESCRIPTOR_SIGNATURE,
              sizeof(pDescriptor->magic),
              sizeof(VOLUME_DESCRIPTOR_SIGNATURE));

    pDescriptor->type = VOLUME_SET_TERMINATOR;
    pDescriptor->version = 1;
}

u16 ISOWriter::big_endian(u16 value) {

    unsigned char b1, b2;

    b1 = (unsigned char) (value & 255);
    b2 = (unsigned char) ((value >> 8) & 255);

    return (b1 << 8) + b2;
}

u32 ISOWriter::big_endian(u32 value) {

    unsigned char b1, b2, b3, b4;

    b1 = (unsigned char) (value & 255);
    b2 = (unsigned char) (( value >> 8 ) & 255);
    b3 = (unsigned char) (( value >> 16 ) & 255);
    b4 = (unsigned char) (( value >> 24 ) & 255);

    return (u32) (((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4);
}

void ISOWriter::set_path_table_content(e_endianness endian, file_tree_entry *tree)
{
    for (unsigned int i = 0; i < file_tree_depth; i++)
    {
        set_path_table_entry(i, 0, tree, endian);
    }
}

void ISOWriter::set_root_path_entry(const e_endianness &endian) {
    char name[2];
    path_entry entry;

    memset(&entry, 0, sizeof(entry));
    memset(&name, 0, sizeof(name));

    entry.name_length = 1;
    entry.extended_length = 0;

    switch (endian)
    {
        case LITTLE_END:
        {
            fseek(fstream, PATH_TABLE_LOCATION_LSB * SECTOR_SIZE, SEEK_SET);

            entry.location = ROOT_ENTRY_LOCATION;
            entry.parent = 1;
            break;
        }

        case BIG_END:
        {
            fseek(fstream, PATH_TABLE_LOCATION_MSB * SECTOR_SIZE, SEEK_SET);

            entry.location = big_endian((u32)ROOT_ENTRY_LOCATION);
            entry.parent = big_endian((u16)1);
            break;
        }

        default: break;
    }

    fwrite(&entry, sizeof(path_entry), 1, fstream);
    fwrite(&name, sizeof(name), 1, fstream);
}

void ISOWriter::set_dir_entries()
{
    global_offset = 0;
    fseek(fstream, ROOT_ENTRY_LOCATION * SECTOR_SIZE, SEEK_SET);
    char self_name = 0;
    char parent_name = 1;
    directory_record root_record;

    memset(&root_record, 0, sizeof(directory_record));
    root_record = get_dir_record(*file_tree);

    fwrite(&root_record, sizeof(directory_record), 1, fstream);
    fwrite(&self_name, sizeof(self_name), 1, fstream);

    fwrite(&root_record, sizeof(directory_record), 1, fstream);
    fwrite(&parent_name, sizeof(parent_name), 1, fstream);

    set_record_recursive(file_tree, &root_record);
}

int ISOWriter::create_file_tree(char **files, int filescount)
{
    char*path;
    bool depth_flag = false;
    struct stat st;
    file_tree_entry entry;
    global_offset = 2;
    unsigned int dircount = 2;

    file_tree = new file_tree_entry;

    memset(file_tree, 0, sizeof(file_tree_entry));

    file_tree->children = new vector<file_tree_entry>;
    file_tree->is_folder = true;
    file_tree->entry_location = ROOT_ENTRY_LOCATION;
    file_tree->parent_index = 1;
    file_tree->size = SECTOR_SIZE;

    for (int i = 1; i < filescount; i++)
    {
        path = files[i];
        if (lstat(path, &st) != 0)
        {
            printf("Error opening the file: %s\n", path);
            return -1;
        }
        if (S_ISDIR(st.st_mode))
        {
            this->dir_count++;
            dircount++;
        }
    }

    for (int i = 1; i < filescount; i++)
    {
        path = files[i];
        char* ind = strrchr(path, '/');

        if (ind == nullptr)
        {
            strcpy(entry.name, path);
        }else {
            strncpy(entry.name, ind + 1, path + strlen(path) - ind);
        }
        get_iso_name(entry.name);
        lstat(path, &st);

        strcpy(entry.path, path);
        entry.size = get_size(path);

        entry.parent_index = 1;
        entry.is_folder = false;

        if(S_ISDIR(st.st_mode))
        {
            entry.entry_location = (u32)(FIRST_DIR_ENTRY_LOCATION + (global_offset - 2) * 2);
            global_offset++;

            inc_path_table_len(entry);

            entry.is_folder = true;
            entry.children = new vector<file_tree_entry>;

            if (!depth_flag)
            {
                depth_flag = true;
                file_tree_depth++;
            }
            int res = build_file_tree_recursive(path, entry.children, dircount, entry.entry_location);
            if (res == -1) return -1;
        }
        get_entry_size(entry);
        file_tree->children->push_back(entry);
    }
    return 0;
}

void ISOWriter::inc_path_table_len(const file_tree_entry &entry) {
    path_table_size += sizeof(path_entry) + strlen(entry.name);
    path_table_size += strlen(entry.name) % 2 == 0 ? 0 : 1;
}

int ISOWriter::build_file_tree_recursive(char *name,
                                         std::vector<file_tree_entry, std::allocator<file_tree_entry>> *tree,
                                         unsigned int start_index, unsigned int parent_index)
{
    DIR *dp;
    bool depth_flag = false;
    struct dirent *dir_info;
    struct stat st;
    file_tree_entry entry;
    unsigned int dir_count = 0;

    memset(&entry, 0, sizeof(file_tree_entry));
    //prints path to the current file
    printf ("%s\n", name);

    if (lstat(name, &st) != 0)
    {
        printf("Error opening the file: %s\n", name);
        return -1;
    }
    strcpy(entry.path, name);
    entry.parent_index = parent_index;

    if((dp = opendir(name)) == NULL)
        return -1;

    while ((dir_info = readdir(dp)) != NULL)
    {
        if ((strcmp(dir_info->d_name, "..") != 0) && (strcmp(dir_info->d_name, ".") != 0))
        {
            if (dir_info->d_type == DT_DIR)
            {
                dir_count++;
                this->dir_count++;
            }
        }
    }
    closedir(dp);

    if((dp = opendir(name)) == NULL)
        return -1;

    while ((dir_info = readdir(dp)) != NULL)
    {
        if ((strcmp(dir_info->d_name, "..") != 0) && (strcmp(dir_info->d_name, ".") != 0))
        {
            char new_name[256];
            strcpy(new_name, name);
            char* new_path = strcat(strcat(new_name, "/"), dir_info->d_name);
            strcpy(entry.path, new_path);
            entry.size = get_size(new_path);

            strcpy(entry.name, dir_info->d_name);
            get_iso_name(entry.name);
            if (dir_info->d_type == DT_DIR)
            {
                entry.is_folder = true;
                entry.entry_location = (u32)(FIRST_DIR_ENTRY_LOCATION + (global_offset - 2) * 2);
                global_offset++;

                inc_path_table_len(entry);

                entry.children = new vector<file_tree_entry>;

                if (!depth_flag)
                {
                    depth_flag = true;
                    file_tree_depth++;
                }
                build_file_tree_recursive(new_path, entry.children,
                                          start_index + dir_count, entry.entry_location);
            }
            else
            {
                entry.is_folder = false;
            }
            get_entry_size(entry);
            tree->push_back(entry);
        }
    }
    closedir(dp);
    return 0;
}

size_t ISOWriter::get_size(char *name)
{
    struct stat stat_buf;
    int rc = stat(name, &stat_buf);
    return rc == 0 ? (size_t)stat_buf.st_size : 0;
}

void ISOWriter::set_path_table_entry(unsigned int depth, unsigned int current_depth, file_tree_entry *tree,
                                     e_endianness endian)
{
    if (current_depth < depth)
    {
        for (auto entry : *tree->children)
        {
            if (entry.is_folder)
            {
                set_path_table_entry(depth, current_depth + 1, &entry, endian);
            }
        }
    }
    else
    {
        for (auto entry : *tree->children)
        {
            if (entry.is_folder)
            {
                path_entry p_entry;
                short padding = 0;

                memset(&p_entry, 0, sizeof(p_entry));

                u32 entry_location = entry.entry_location;
                fseek(fstream, global_offset, SEEK_SET);

                switch (endian)
                {
                    case BIG_END:
                    {
                        p_entry.parent = big_endian((u16)entry.parent_index);
                        p_entry.location = big_endian(entry_location);
                        break;
                    }
                    case LITTLE_END:
                    {
                        p_entry.parent = (u16)entry.parent_index;
                        p_entry.location = entry_location;
                        break;
                    }
                    default: break;
                }

                p_entry.name_length = (u8)strlen(entry.name);

                if (strlen(entry.name) % 2 != 0)
                {
                    padding = 1;
                }

                fwrite(&p_entry, sizeof(path_entry), 1, fstream);
                fwrite(&entry.name, p_entry.name_length, 1, fstream);

                global_offset += sizeof(path_entry) + p_entry.name_length + padding;
            }
        }
    }

}

directory_record ISOWriter::get_dir_record(file_tree_entry entry) {

    directory_record record;
    memset(&record, 0, sizeof(directory_record));

    record.length = (u8)(33 + strlen(entry.name));
    record.sequence_number = get_both_endian((u16)VOL_INDEX);
    record.length_file_id = (u8)strlen(entry.name);

    if (entry.is_folder)
    {
        record.location = get_both_endian(entry.entry_location);
        record.data_length = get_both_endian((u32)SECTOR_SIZE);
        record.flags = FLAG_DIRECTORY;
    }
    else
    {
        record.data_length = get_both_endian((u32)entry.size);
        record.location =  get_both_endian((u32)(global_offset + FIRST_DIR_ENTRY_LOCATION + (dir_count + 1) * 2));
        global_offset += get_entry_size(entry);
    }

    if (entry.entry_location == ROOT_ENTRY_LOCATION)
    {
        record.length = 34;
        record.length_file_id = 1;
    }

    return record;
}

void ISOWriter::set_record_recursive(file_tree_entry *entry, directory_record *parent_record)
{

    char self_name = 0;
    char parent_name = 1;
    directory_record record;
    int padding =  0;

    //set child records for previous call.
    for (auto child : *entry->children)
    {
        record = get_dir_record(child);
        padding = strlen(child.name) % 2 == 0 ? 1 : 0;
        record.length += padding;
        fwrite(&record, sizeof(directory_record), 1, fstream);
        fwrite(child.name, strlen(child.name) + padding, 1, fstream);
        copy_content(child, record.location.little);
    }

    for (auto child : *entry->children)
    {
        if (child.is_folder)
        {
            record = get_dir_record(child);
            fseek(fstream, child.entry_location * SECTOR_SIZE, SEEK_SET);

            record.length = 34;
            record.length_file_id = 1;

            fwrite(&record, sizeof(directory_record), 1, fstream);
            fwrite(&self_name, 1, 1, fstream);

            fwrite(parent_record, sizeof(directory_record), 1, fstream);
            fwrite(&parent_name, 1, 1, fstream);

            set_record_recursive(&child, &record);
        }
    }
}

long ISOWriter::get_entry_size(file_tree_entry entry)
{
    if (!entry.is_folder)
    {
        long block_count = entry.size / SECTOR_SIZE;
        if (entry.size % SECTOR_SIZE != 0)
        {
            block_count++;
        }
        this->total_block_size += block_count;
        return block_count;
    }

    return 1;
}

void ISOWriter::copy_content(file_tree_entry entry, long location)
{
    if (entry.is_folder) return;
    FILE * input = fopen(entry.path, "rb");
    if(!input) return;


    size_t write_size;
    char buffer[SECTOR_SIZE];
    long restore = ftell(fstream);
    fseek(fstream, location * SECTOR_SIZE, SEEK_SET);
    size_t size = get_size(entry.path);

    //Read and write each bytes block
    while(size)
    {
        write_size = size > SECTOR_SIZE? SECTOR_SIZE: size;

        fread(buffer, write_size, 1, input);
        fwrite(buffer, write_size, 1, fstream);

        size -= write_size;
    };

    fseek(fstream, restore, SEEK_SET);
    fclose(input);
}

void ISOWriter::destroy_tree_recursive(file_tree_entry *tree) {
    for (auto entry: *tree->children)
    {
        if (entry.is_folder)
        {
            destroy_tree_recursive(&entry);
            delete entry.children;
        }
    }
}

void ISOWriter::get_iso_name(char *pString)
{
    for (int i = 0; i < strlen(pString); i++)
    {
        pString[i] = (char)toupper(pString[i]);
        if (!is_iso_char(pString[i]))
            pString[i] = NAME_FILLER;
    }
}

bool ISOWriter::is_iso_char(char c) {
    return true;
//    return  ((c >= 'A') && (c <= 'Z')) ||
//            ((c >= 'a') && (c <= 'z')) ||
//            ((c >= '0') && (c <= '9')) ||
//            (c == '.');
}
