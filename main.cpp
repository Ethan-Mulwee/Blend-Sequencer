#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <exception>
#include <cstdint>


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

    SDNA *sdna;
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

void Int32ToChar(char a[], int32_t n) {
    memcpy(a, &n, sizeof(int32_t));
}

int32_t CharToInt32(char a, char b, char c, char d) {
    return (int32_t(d) << 24 | int32_t(c) << 16 | int32_t(b) << 8 | int32_t(a));
}

const char* PadTo4(const char* ptr) {
    return (const char*)((uintptr_t(ptr) + 3) & ~3);
}

/* See dna_utils.cc */
int DNA_member_array_num(const char *str) {
    int result = 1;
    int current = 0;
    while (true) {
        char c = *str++;
        switch(c) {
            case '\0':
                return result;
            case '[':
                current = 0;
                break;
            case ']':
                result *= current;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                current = current * 10 + (c - '0');
                break;
            default:
                break;
        }
    }
}

/* See init_structDNA() in dna_genfile.cc, called after ReadSDNA fetches the data and stores it in sdna->data */
void InitalizeSDNA(SDNA *sdna) {
    int *data_pointer = (int*)sdna->data;

    sdna->types = nullptr;
    sdna->types_size = nullptr;
    sdna->types_alignment = nullptr;
    sdna->structs = nullptr;
    // sdna->type_to_structs_map = nullptr;
    sdna->members = nullptr;
    sdna->members_array_num = nullptr;

    if (*data_pointer != CharToInt32('S', 'D', 'N', 'A')) {
        throw std::runtime_error("Error reading SDNA header");
    }

    data_pointer++;

    /* --------------------------------- Members -------------------------------- */

    /* Initalize member name array */
    if (*data_pointer == CharToInt32('N', 'A', 'M', 'E')) {
        data_pointer++;
        sdna->members_num = *data_pointer;
        sdna->members_num_alloc = sdna->members_num;

        data_pointer++;
        sdna->members = new const char*[sdna->members_num];
        // Zero new array
        memset(sdna->members, 0, sdna->members_num * sizeof(const char*));

        sdna->members_array_num = new short[sdna->members_num];
    } else {
        throw std::runtime_error("Error reading SDNA NAME header");
    }

    /* Find member names */
    const char *member_pointer = (char*)data_pointer;
    for (int member_index = 0; member_index < sdna->members_num; member_index++) {
        sdna->members[member_index] = member_pointer;

        /* Keep going until you find null terminator */
        while(*member_pointer) {
            member_pointer++;
        }
        /* Go to next member name */
        member_pointer++;
    }

    member_pointer = PadTo4(member_pointer);

    /* Find array numbers */
    for (int member_index = 0; member_index < sdna->members_num; member_index++) {
        sdna->members_array_num[member_index] = DNA_member_array_num(sdna->members[member_index]); 
    }

    /* ---------------------------------- Types --------------------------------- */

    /* Initalize type name arrays */
    data_pointer = (int*)member_pointer;
    if (*data_pointer == CharToInt32('T', 'Y', 'P', 'E')) {
        data_pointer++;

        sdna->types_num = *data_pointer;

        data_pointer++;
        sdna->types = new const char*[sdna->types_num];
        // Zero new array
        memset(sdna->types, 0, sdna->types_num * sizeof(const char*));
    } else {
        throw std::runtime_error("Error reading SDNA TYPE header");
    }

    /* Find type names */
    const char* type_pointer = (char*)data_pointer;
    for (int type_index = 0; type_index < sdna->types_num; type_index++) {
        sdna->types[type_index] = type_pointer;
        
        /* Keep going until you find null terminator */
        while (*type_pointer) {
            type_pointer++;  
        }
        /* Go to next type name */
        type_pointer++;
    }

    type_pointer = PadTo4(type_pointer);

    /* --------------------------- Type Length Arrray --------------------------- */
    /* Array is already in the data memory properly so no need to loop over this one */
    data_pointer = (int*)type_pointer;
    short* type_length_pointer;
    if (*data_pointer == CharToInt32('T', 'L', 'E', 'N')) {
        data_pointer++;
        type_length_pointer = (short*)data_pointer;

        sdna->types_size = type_length_pointer;
        type_length_pointer += sdna->types_num;
    } else {
        throw std::runtime_error("Error reading SDNA TYPE LENGTH (TLEN) header");
    }

    /* prevent BUS error? honestly not sure why this is here */
    if (sdna->types_num & 1) {
        type_length_pointer++;
    }

    /* ------------------------------ Struct Array ------------------------------ */
    data_pointer = (int*)type_length_pointer;
    if (*data_pointer == CharToInt32('S', 'T', 'R', 'C')) {
        data_pointer++;
        sdna->structs_num = *data_pointer;
        data_pointer++;

        sdna->structs = new SDNA_Struct*[sdna->structs_num];
        memset(sdna->structs, 0, sdna->structs_num * sizeof(SDNA_Struct));
    } else {
        throw std::runtime_error("Error reading SDNA STRCUT ARRAY (STRC) header");
    }

    /* 
     * Blender does a check to ensure the same struct index isn't used twice here 
     * but I'm not going to bother at least for now
     */

    short* struct_pointer = (short*)data_pointer;
    for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
        SDNA_Struct *struct_info = (SDNA_Struct*)struct_pointer;
        sdna->structs[struct_index] = struct_info;

        struct_pointer += 2 + (sizeof(SDNA_StructMember) / sizeof(short)) * struct_info->members_num;
    }

    /* 
     * Here pointer_size is normally calculated but since this is for blender 5.0 
     * 64bit or 8 bytes is assumed by this library 
     */

    
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

    result.sdna = ReadSDNA(file, result.block_header_list.last->perv->file_offset, result.block_header_list.last->perv->block_header.len);

    InitalizeSDNA(result.sdna);

    return result;
}



int main() {

    BlendFile blend_file = ReadBlendFile("Cube.blend");

    std::cout << "header: " << blend_file.header << "\n";
    std::cout << "length: " << blend_file.header_length << "\n";
    std::cout << "format version: " << blend_file.format_version << "\n";
    std::cout << "blender version: " << blend_file.blender_version << "\n";

    BlockHeaderNode* node = blend_file.block_header_list.first;
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

    // std::cout.write(blendFile.file_SDNA->data, blendFile.file_SDNA->data_size);

    std::cout << "\nNumber of members: " << blend_file.sdna->members_num << "\nMembers: \n";
    for (int i = 0; i < blend_file.sdna->members_num; i++) {
        std::cout << blend_file.sdna->members[i] << ", Array length: " << blend_file.sdna->members_array_num[i] << "\n";
    }

    std::cout << "\nNumber of types: " << blend_file.sdna->types_num << "\nTypes: \n";
        for (int i = 0; i < blend_file.sdna->types_num; i++) {
        std::cout << blend_file.sdna->types[i] << "\n";
    }

    std::cout << "\nNumber of structs: " << blend_file.sdna->structs_num << "\nTypes: \n";
        for (int i = 0; i < blend_file.sdna->structs_num; i++) {
            SDNA_Struct* struct_pointer = blend_file.sdna->structs[i];
            std::cout << "Members num:" << struct_pointer->members_num << "\n";
            std::cout << "Members: \n";
            for (int member_index = 0; member_index < struct_pointer->members_num; member_index++) {
                SDNA_StructMember member = struct_pointer->members[member_index];
                std::cout << "(Type): " << blend_file.sdna->types[member.type_index] << ", ";
                std::cout << "(Name): " << blend_file.sdna->members[member.member_index] << "\n";
            }
            std::cout << "Type index:" << struct_pointer->type_index << "\n";
            std::cout << "Type name:" << blend_file.sdna->types[struct_pointer->type_index] << "\n";
            std::cout << "\n\n";
    }
}