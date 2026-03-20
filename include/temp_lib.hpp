#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <exception>
#include <cstdint>
#include <map>
#include <vector>

#include "sdna_structs.h"
#include "attribute_types.h"

/* -------------------------------------------------------------------------- */
/*                                BLOCK HEADER                                */
/* -------------------------------------------------------------------------- */

/* Aka BHead or LargeBHead8 in blender source */
struct DataBlockHeader {
    int32_t code;
    // SDNAnr
    int32_t SDNA_type_index;
    uint64_t old_pointer;
    // len
    int64_t byte_length;
    // nr
    int64_t struct_number;
};

/* Aka BHeadN */
struct DataBlockNode {
    DataBlockNode *next, *perv;
    uint64_t data_offset;
    // bool has_data;
    DataBlockHeader block_header;
};

struct BlockHeaderList {
    DataBlockNode* first = nullptr;
    DataBlockNode* last = nullptr;

    void add(DataBlockNode* header) {
        if (first) {
            header->perv = last;
            last->next = header;
            last = header;
        } else {
            first = header;
            last = header;
        }
    }
};

/* -------------------------------------------------------------------------- */
/*                                    SDNA                                    */
/* -------------------------------------------------------------------------- */

/* 
 * Types are null termianted c strings in the style of C like "vertex[3]\0" 
 */

struct SDNA_StructMember {
    short type_index;
    short member_index;
};

struct SDNA_Struct {
    short type_index;
    short members_num;
    SDNA_StructMember members[];
};

/* Structure DNA largely mirrors SDNA def from blender source */
struct SDNA {
    /* Encoded data from before parsing */
    int data_size;
    bool data_alloc;

    /* ---------------------------------- TYPES --------------------------------- */

    int types_num;
    /* Type names */
    const char **types;
    short *types_size;
    int *types_alignment;

    /* ---------------------------------- TYPES --------------------------------- */

    /* --------------------------------- STRUCTS -------------------------------- */

    int structs_num;
    SDNA_Struct **structs;

    /* --------------------------------- STRUCTS -------------------------------- */

    /* ----------------------------- STRUCT MEMBERS ----------------------------- */

    /* Total number of struct members */
    int members_num;
    /* Unused for the moment */
    int members_num_alloc;

    /* Struct member names */
    const char **members;

    /* 
     * parallel array to members, if a member is an array like float[2] 
     * this stores the length i.e. 2 otherwise it stores a 1 for any other member 
     */
    short *members_array_num;

    /* ----------------------------- STRUCT MEMBERS ----------------------------- */

    /* 
     * TODO: here goes a map that maps between type names to struct indices
     * see SDNA Ghash array in blender source
     */ 

};

struct BlendFile {
    const char *data;
    size_t data_length;

    char* header;
    int header_length;
    int format_version;
    int blender_version;

    BlockHeaderList block_header_list;

    /* TODO: replace this with a more efficent structure later */
    std::map<void*, DataBlockNode*> pointer_to_block_map;

    SDNA *sdna;

    template<typename T>
    T ReadRawDataAs(size_t data_index) {
        return *((T*)&data[data_index]);
    }

    const char* GetRawDataAddress(size_t data_index) {
        return &data[data_index];
    }

    template<typename T>
    T* MapPointer(T* ptr) {
        DataBlockNode* data_block = pointer_to_block_map[ptr];
        T* mapped_pointer = (T*)GetRawDataAddress(data_block->data_offset);
        return mapped_pointer;
    }

    template<typename T>
    DataBlockNode* MapPointerToBlock(T* ptr) {
        DataBlockNode* data_block = pointer_to_block_map[ptr];
        return data_block;
    }

    const char* TypeNameFromStructSDNAIndex(uint32_t SDNAnr) const {
        short data_type_index = sdna->structs[SDNAnr]->type_index;
        const char* data_type = sdna->types[data_type_index];
        return data_type;
    }

        const char* TypeNameOfDataBlock(DataBlockNode* node) const {
        short data_type_index = sdna->structs[node->block_header.SDNA_type_index]->type_index;
        const char* data_type = sdna->types[data_type_index];
        return data_type;
    }
};




/* Structure designed to simplify reading and writing blend files  */
struct IntermediateBlendFile;

void Int32ToChar(char a[], int32_t n);

int32_t CharToInt32(char a, char b, char c, char d);

// + 1 and - 1 are needed because the data starts at 0 and not 1
size_t PadTo4(size_t index);

/* See dna_utils.cc */
int DNA_member_array_num(const char *str);

/* See init_structDNA() in dna_genfile.cc, called after ReadSDNA fetches the data and stores it in sdna->data */
SDNA *ReadSDNA(BlendFile file, uint64_t data_index);

 template<typename T>
T ReadDataBlock(const BlendFile& blend_file, const DataBlockNode* node, int offset);

template<typename T>
T ReadDataAs(void* data);

BlendFile ReadBlendFile(const char* path);

void ExtractSDNATypesToHeaderFile(const BlendFile& blend_file);

void InterpretDataBlocks(BlendFile& blend);

void LogBlendFileHeader(const BlendFile& blend);

void LogDataBlocks(const BlendFile& blend);