//
// Created by vladkanash on 10/22/15.
//
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "types.h"

#ifndef GBISO_ISOWRITER_H
#define GBISO_ISOWRITER_H


class ISOWriter {

private:

    FILE* fstream;

    file_tree_entry* file_tree;

    unsigned int dir_count;

    unsigned int path_table_size;

    unsigned int total_block_size;

    unsigned int file_tree_depth;

    long global_offset;

public:

    ISOWriter(char **files, int filecount);

    ~ISOWriter();

    int write_iso(const char *filename);

private:

    void set_pvd(primary_volume_descriptor *pDescriptor);

    void set_value(char *field, const char* value, const size_t field_size, const size_t value_size, const char filler = 0);

    void set_pvd_time(primary_volume_descriptor *pDescriptor);

    void itoa(char *dest, const int src) const;

    void set_tvd(volume_descriptor *pDescriptor);

    u32 big_endian(u32 value);

    both_endian_16 get_both_endian(u16 value);

    u16 big_endian(u16 value);

    both_endian_32 get_both_endian(u32 value);

    void set_dir_entries();

    int create_file_tree(char **files, int filescount);

    int build_file_tree_recursive(char *name,
                                  std::vector<file_tree_entry, std::allocator<file_tree_entry>> *pVector,
                                  unsigned int index, unsigned int parent_index);

    size_t get_size(char *name);

    void set_root_path_entry(const e_endianness &endian);

    void set_path_table_content(e_endianness endian, file_tree_entry *tree);

    void set_path_table_entry(unsigned int depth, unsigned int current_depth, file_tree_entry *tree,
                              e_endianness endian);

    directory_record get_dir_record(file_tree_entry entry);

    void set_record_recursive(file_tree_entry *pEntry, directory_record *entry);

    long get_entry_size(file_tree_entry entry);

    void copy_content(file_tree_entry entry, long location);

    void inc_path_table_len(const file_tree_entry &entry);

    void create_blank_file() const;

    void set_volume_descriptors();

    void set_path_table();

    void destroy_tree_recursive(file_tree_entry *tree);

    void get_iso_name(char *pString);

    bool is_iso_char(char string);
};


#endif //GBISO_ISOWRITER_H
