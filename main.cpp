#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <exception>


/* -------------------------------------------------------------------------- */
/*                                BLOCK HEADER                                */
/* -------------------------------------------------------------------------- */

/* Aka BHead or LargeBHead8 in blender source */
struct BlockHeader {
    int32_t code;
    int32_t SDNAnr;
    uint64_t old_pointer;
    int64_t len;
    int64_t nr;
};

/* Aka BHeadN */
struct BlockHeaderNode {
    BlockHeaderNode *next, *perv;
    uint64_t file_offset;
    bool has_data;
    BlockHeader block_header;
};

struct BlockHeaderList {
    BlockHeaderNode* first = nullptr;
    BlockHeaderNode* last = nullptr;

    void add(BlockHeaderNode* header) {
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
    const char *data;
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
    char* header;
    int header_length;
    int format_version;
    int blender_version;

    BlockHeaderList block_header_list;

    SDNA *file_SDNA;
};

// const char* ReadBlock()

SDNA *ReadSDNA(std::ifstream& file, uint64_t data_offset, uint64_t data_length) {
    SDNA *sdna = new SDNA();

    sdna->data_size = data_length;
    char* data = new char[data_length];
    file.seekg(data_offset);
    file.read(data, data_length);

    sdna->data = data;

    return sdna;
}



BlendFile ReadBlendFile(const char* path) {
    BlendFile result;

    std::ifstream file(path, std::ios::in | std::ios::binary);

    /* ------------------------------- READ HEADER ------------------------------ */
    /* hardcoded because this isn't really expected to change */
    int header_length = 17;
    char* header = new char[header_length + 1];
    header[header_length + 1] = '\0';

    file.read(header, 17);

    if (!file) {
        throw std::runtime_error("Failed to open file");
    }

    char format_version_cstr[2] = {header[10], header[11]};
    int format_version = atoi(format_version_cstr);

    char blender_version_cstr[4] = {header[14], header[15], header[16], header[17]};
    int blender_version = atoi(blender_version_cstr);

    /* ------------------------------- SET HEADER ------------------------------- */
    result.header = header;
    result.header_length = header_length;
    result.format_version = format_version;
    result.blender_version = blender_version;

    /* ---------------------------- READ BLOCK HEADER --------------------------- */


    // based on get_bhead()
    while(true) {
        BlockHeader block_header; 
        file.read((char*)&block_header, sizeof(BlockHeader));
        int file_offset = file.tellg();
        if (file.eof()) {
            break;
        }

        BlockHeaderNode* block_header_node = new BlockHeaderNode();
        block_header_node->next = nullptr;
        block_header_node->perv = nullptr;
        block_header_node->file_offset = file_offset;
        block_header_node->block_header = block_header;

        result.block_header_list.add(block_header_node);

        file.seekg(block_header.len, std::ios::seekdir::_S_cur);
    }

    file.clear();

    result.file_SDNA = ReadSDNA(file, result.block_header_list.last->perv->file_offset, result.block_header_list.last->perv->block_header.len);

    return result;
}

void Int32ToChar(char a[], int32_t n) {
    memcpy(a, &n, sizeof(int32_t));
}

int main() {

    BlendFile blendFile = ReadBlendFile("Cube.blend");

    std::cout << "header: " << blendFile.header << "\n";
    std::cout << "length: " << blendFile.header_length << "\n";
    std::cout << "format version: " << blendFile.format_version << "\n";
    std::cout << "blender version: " << blendFile.blender_version << "\n";

    BlockHeaderNode* node = blendFile.block_header_list.first;
    while(node) {
        const BlockHeader& block_header = node->block_header;
        std::cout << "\nblock\n";

        char code_cstr[sizeof(uint32_t)+ 1];
        code_cstr[sizeof(uint32_t)] = '\0';
        Int32ToChar(code_cstr, block_header.code);
        std::cout << "code:" << code_cstr << "\n";

        std::cout << "SDNA struct type: " << block_header.SDNAnr << "\n";
        std::cout << "old pointer: " << block_header.old_pointer << "\n";
        std::cout << "byte length: " << block_header.len << "\n";
        std::cout << "number of structs: " << block_header.nr << "\n";

        node = node->next;
    }

    std::cout.write(blendFile.file_SDNA->data, blendFile.file_SDNA->data_size);
}