#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <exception>
#include <cstdint>
#include <map>

#include "sdna_structs.h"
#include "attribute_types.h"

/* See BlendFileReader.py */

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
struct IntermediateBlendFile {

};

void Int32ToChar(char a[], int32_t n) {
    memcpy(a, &n, sizeof(int32_t));
}

int32_t CharToInt32(char a, char b, char c, char d) {
    return (int32_t(d) << 24 | int32_t(c) << 16 | int32_t(b) << 8 | int32_t(a));
}

// const char* PadTo4(const char* ptr) {
//     return (const char*)((uintptr_t(ptr) + 3) & ~3);
// }

// + 1 and - 1 are needed because the data starts at 0 and not 1
size_t PadTo4(size_t index) {
    return (size_t)(((index + 1) + 3) & ~3) - 1;
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
SDNA *ReadSDNA(BlendFile file, uint64_t data_index) {
    SDNA *sdna = new SDNA();

    sdna->types = nullptr;
    sdna->types_size = nullptr;
    sdna->types_alignment = nullptr;
    sdna->structs = nullptr;
    // sdna->type_to_structs_map = nullptr;
    sdna->members = nullptr;
    sdna->members_array_num = nullptr;

    if (file.ReadRawDataAs<int>(data_index) != CharToInt32('S', 'D', 'N', 'A')) {
        throw std::runtime_error("Error reading SDNA header");
    }

    data_index += sizeof(int);

    /* --------------------------------- Members -------------------------------- */

    /* Initalize member name array */
    if (file.ReadRawDataAs<int>(data_index) == CharToInt32('N', 'A', 'M', 'E')) {
        data_index += sizeof(int);
        sdna->members_num = file.ReadRawDataAs<int>(data_index);
        data_index += sizeof(int);
        sdna->members_num_alloc = sdna->members_num;

        sdna->members = new const char*[sdna->members_num];
        // Zero new array
        memset(sdna->members, 0, sdna->members_num * sizeof(const char*));

        sdna->members_array_num = new short[sdna->members_num];
    } else {
        throw std::runtime_error("Error reading SDNA NAME header");
    }

    /* Find member names */
    for (int member_index = 0; member_index < sdna->members_num; member_index++) {
        const char *member_pointer = file.GetRawDataAddress(data_index);
        sdna->members[member_index] = member_pointer;

        /* Keep going until you find null terminator */
        while(*member_pointer) {
            member_pointer++;
            data_index++;
        }
        /* Go to next member name */
        data_index++;
    }

    data_index = PadTo4(data_index);

    /* Find array numbers */
    for (int member_index = 0; member_index < sdna->members_num; member_index++) {
        sdna->members_array_num[member_index] = DNA_member_array_num(sdna->members[member_index]); 
    }

    /* ---------------------------------- Types --------------------------------- */

    /* Initalize type name arrays */
    if (file.ReadRawDataAs<int>(data_index) == CharToInt32('T', 'Y', 'P', 'E')) {
        data_index += sizeof(int);

        sdna->types_num = file.ReadRawDataAs<int>(data_index);
        data_index += sizeof(int);

        sdna->types = new const char*[sdna->types_num];
        // Zero new array
        memset(sdna->types, 0, sdna->types_num * sizeof(const char*));
    } else {
        throw std::runtime_error("Error reading SDNA TYPE header");
    }

    /* Find type names */
    for (int type_index = 0; type_index < sdna->types_num; type_index++) {
        const char* type_pointer = file.GetRawDataAddress(data_index);
        sdna->types[type_index] = type_pointer;
        
        /* Keep going until you find null terminator */
        while (*type_pointer) {
            type_pointer++;  
            data_index++;
        }
        /* Go to next type name */
        data_index++;
    }

    data_index = PadTo4(data_index);

    /* --------------------------- Type Length Arrray --------------------------- */
    /* Array is already in the data memory properly so no need to loop over this one */
    short* type_length_pointer;
    if (file.ReadRawDataAs<int>(data_index) == CharToInt32('T', 'L', 'E', 'N')) {
        data_index += sizeof(int);
        type_length_pointer = (short*)file.GetRawDataAddress(data_index);

        sdna->types_size = type_length_pointer;
        data_index += sdna->types_num * sizeof(short);
    } else {
        throw std::runtime_error("Error reading SDNA TYPE LENGTH (TLEN) header");
    }

    /* prevent BUS error? honestly not sure why this is here */
    if (sdna->types_num & 1) {
        data_index += sizeof(short);
    }

    /* ------------------------------ Struct Array ------------------------------ */
    if (file.ReadRawDataAs<int>(data_index) == CharToInt32('S', 'T', 'R', 'C')) {
        data_index += sizeof(int);
        sdna->structs_num = file.ReadRawDataAs<int>(data_index);
        data_index += sizeof(int);

        sdna->structs = new SDNA_Struct*[sdna->structs_num];
        memset(sdna->structs, 0, sdna->structs_num * sizeof(SDNA_Struct));
    } else {
        throw std::runtime_error("Error reading SDNA STRCUT ARRAY (STRC) header");
    }

    /* 
     * Blender does a check to ensure the same struct index isn't used twice here 
     * but I'm not going to bother at least for now
     */

    short* struct_pointer = (short*)file.GetRawDataAddress(data_index);
    for (int struct_index = 0; struct_index < sdna->structs_num; struct_index++) {
        SDNA_Struct *struct_info = (SDNA_Struct*)struct_pointer;
        sdna->structs[struct_index] = struct_info;

        struct_pointer += 2 + (sizeof(SDNA_StructMember) / sizeof(short)) * struct_info->members_num;
    }

    /* 
     * Here pointer_size is normally calculated but since this is for blender 5.0 
     * 64bit or 8 bytes is assumed by this library 
     */


    /* ----------------------------- Type Alignment ----------------------------- */
    sdna->types_alignment = new int[sdna->types_num];
    for (int type_index = 0; type_index < sdna->types_num; type_index++) {
       sdna->types_alignment[type_index] = int(16UL);
    }

    return sdna;
}

 template<typename T>
T ReadDataBlock(const BlendFile& blend_file, const DataBlockNode* node, int offset) {
    return *((T*)&blend_file.data[node->data_offset + (sizeof(T)*offset)]);
}

template<typename T>
T ReadDataAs(void* data) {
    return *((T*)data);
}

BlendFile ReadBlendFile(const char* path) {
    BlendFile file;

    std::ifstream file_reader(path, std::ios::in | std::ios::binary);

    if (!file_reader) {
        throw std::runtime_error("Failed to open file");
    }

    /* --------------------------- READ RAW FILE DATA --------------------------- */

    file_reader.seekg(0, std::ios::end);
    size_t data_length = file_reader.tellg();

    file_reader.seekg(0, std::ios::beg);
    
    char* data = new char[data_length];
    file_reader.read(data, data_length);
    file.data = data;
    file.data_length = data_length;

    file_reader.close();

    /* ------------------------------- READ HEADER ------------------------------ */
    /* hardcoded because this isn't really expected to change */
    size_t data_index = 0;

    int header_length = 17;
    char* header = &data[0];
    data_index += header_length;

    char format_version_cstr[2] = {header[10], header[11]};
    int format_version = atoi(format_version_cstr);

    char blender_version_cstr[4] = {header[14], header[15], header[16], header[17]};
    int blender_version = atoi(blender_version_cstr);

    /* ------------------------------- SET HEADER ------------------------------- */
    file.header = header;
    file.header_length = header_length;
    file.format_version = format_version;
    file.blender_version = blender_version;

    /* ------------------------- READ DATA BLOCK HEADERS ------------------------ */

    while(data_index < data_length) {
        DataBlockHeader block_header = file.ReadRawDataAs<DataBlockHeader>(data_index);
        data_index += sizeof(DataBlockHeader);

        DataBlockNode* block_node = new DataBlockNode();
        block_node->next = nullptr;
        block_node->perv = nullptr;
        block_node->data_offset = data_index;
        block_node->block_header = block_header;

        file.block_header_list.add(block_node);

        file.pointer_to_block_map.insert({(void*)block_header.old_pointer, block_node});

        data_index += block_header.byte_length;
    }

    file.sdna = ReadSDNA(file, file.block_header_list.last->perv->data_offset);

    return file;
}

void ExtractSDNATypesToHeaderFile(const BlendFile& blend_file) {
    std::fstream file("sdna_structs.h", std::ios::out);

    file << "#include <cstdint>\n\n";

    /* forward declaration to avoid ordering issues */
    for (int i = 0; i < blend_file.sdna->structs_num; i++) {
        SDNA_Struct* struct_pointer = blend_file.sdna->structs[i];
        file << "struct " << blend_file.sdna->types[struct_pointer->type_index] << ";\n";
    }

    file << "\n";

    /* These members have size zero and are almost exclusively just named pointers */
    for (int i = 0; i < blend_file.sdna->types_num; i++) {
        const char* type = blend_file.sdna->types[i];
        if ((strcmp(type, "void") == 0) || (strcmp(type, "bool") == 0) || (strcmp(type, "raw_data") == 0)) {
            continue;
        }
        if (blend_file.sdna->types_size[i] == 0) {
            file << "typedef void " << type << ";\n";
        }
    }
    file << "typedef unsigned short ushort;\n";
    file << "typedef unsigned char uchar;\n";

    file << "\n";

    /* TODO: sorting system to avoid ordering issues */

    for (int i = 0; i < blend_file.sdna->structs_num; i++) {
        SDNA_Struct* struct_pointer = blend_file.sdna->structs[i];
        file << "struct " << blend_file.sdna->types[struct_pointer->type_index] << " { \n";
        for (int member_index = 0; member_index < struct_pointer->members_num; member_index++) {
            SDNA_StructMember member = struct_pointer->members[member_index];
            
            file << "\t" << blend_file.sdna->types[member.type_index] << " ";
            file << "" << blend_file.sdna->members[member.member_index] << ";\n";
        }
        file << "}; \n\n";
    }

    file.close();
}

void InterpretDataBlocks(const BlendFile& blend) {
    DataBlockNode* node = blend.block_header_list.first;
    int uncovered_cases = 0;
    while(node) {
        // switch on type index
        switch (node->block_header.SDNA_type_index) {

            case 321: { // Mesh
                std::cout << "Found a mesh \n";
                break;
            }

            default:
                uncovered_cases++;
                break;
        }

        
        node = node->next;
    }
    std::cout << "Interpret missed " << uncovered_cases << " data blocks" << "\n";
}

void LogBlendFileHeader(const BlendFile& blend) {
    std::cout << "header: " << blend.header << "\n";
    std::cout << "length: " << blend.header_length << "\n";
    std::cout << "format version: " << blend.format_version << "\n";
    std::cout << "blender version: " << blend.blender_version << "\n";
}

void LogDataBlocks(const BlendFile& blend) {
    DataBlockNode* node = blend.block_header_list.first;
    while(node) {
        const DataBlockHeader& block_header = node->block_header;
        std::cout << "\nblock\n";

        char code_cstr[sizeof(uint32_t)+ 1];
        code_cstr[sizeof(uint32_t)] = '\0';
        Int32ToChar(code_cstr, block_header.code);
        std::cout << "code:" << code_cstr << "\n";

        std::cout << "SDNA struct type: " << block_header.SDNA_type_index << "\n";
        // SDNA_Struct* struct_info = blend.sdna->structs[block_header.SDNAnr];
        // std::cout << "Struct type name: " << blend.sdna->types[struct_info->type_index] << "\n";
        std::cout << "old pointer: " << block_header.old_pointer << "\n";
        std::cout << "byte length: " << block_header.byte_length << "\n";
        std::cout << "number of structs: " << block_header.struct_number << "\n";
        std::cout << "data_index: " << node->data_offset << "\n";

        node = node->next;
    }
}

int main() {

    BlendFile blend_file = ReadBlendFile("Cube.blend");
    // ExtractSDNATypesToHeaderFile(blend_file);

    
    // LogBlendFileHeader(blend_file);
    
    // LogDataBlocks(blend_file);
    
    
    /* Attempt to read mesh data */
    /* See DNA_mesh_types.h struct Mesh */


    DataBlockNode* node = blend_file.block_header_list.first;
    while(node) {
        const DataBlockHeader& block_header = node->block_header;
        SDNA_Struct* struct_info = blend_file.sdna->structs[block_header.SDNA_type_index];
        const char* type_name = blend_file.sdna->types[struct_info->type_index];
        if (strcmp(type_name, "Mesh") == 0) {
            Mesh mesh; 
            mesh = ReadDataBlock<Mesh>(blend_file, node, 0);
            std::cout << "SDNA index: " << node->block_header.SDNA_type_index << "\n";
            std::cout << "Mesh Name: " << mesh.id.name << "\n";
            std::cout << "Vertex Count: " << mesh.totvert << "\n";
            std::cout << "Edge Count: " << mesh.totedge << "\n";
            std::cout << "Face Count: " << mesh.totpoly << "\n";
            std::cout << "Corner Count: " << mesh.totloop << "\n";
            std::cout << "Number of Attributes:" << mesh.attribute_storage.dna_attributes_num << "\n";
 
            DataBlockNode* poly_offset_indices_data_block = blend_file.MapPointerToBlock(mesh.poly_offset_indices);
            std::cout << "face_offset_indices Type: " << blend_file.TypeNameOfDataBlock(poly_offset_indices_data_block) << "\n";
            std::cout << "face_offset_indices Byte Length: " << poly_offset_indices_data_block->block_header.byte_length << "\n";
            int* poly_offset_indices = (int*)blend_file.GetRawDataAddress(poly_offset_indices_data_block->data_offset);
            /* See DNA_mesh_types.h faces() */

            /* From the developer docs https://developer.blender.org/docs/features/objects/mesh/mesh/ 
            OffsetIndices is a general abstraction for splitting a larger array into many contiguous groups. 
            Every group is represented by a single integer-- the first index of the elements in the group. 
            The end of the group is simply the next integer in the offsets array. For example, the face offsets 
            [0,3,7,10,14] encode a triangle, a quad, a triangle, and a quad, in that order. */

            /* i.e 7 numbers encode 6 faces explaining the + 1 */
            std::cout << "{";
            for (int poly_offset_indice_idx = 0; poly_offset_indice_idx < mesh.totpoly + 1; poly_offset_indice_idx++) {
                std::cout << poly_offset_indices[poly_offset_indice_idx] << ", ";
            }
            std::cout << "}\n";

            Attribute* attributes = blend_file.MapPointer(mesh.attribute_storage.dna_attributes);
            
            std::cout << "Mesh Attributes: \n";
            for (int attribute_idx = 0; attribute_idx < mesh.attribute_storage.dna_attributes_num; attribute_idx++) {
                Attribute& attribute = attributes[attribute_idx];
                std::cout << "    Name: " << blend_file.MapPointer(attribute.name) << "\n";
                std::cout << "    Type: " << attribute.data_type << "\n";
                std::cout << "    Domain: " << (int)attribute.domain << "\n";

                DataBlockNode* attribute_array_data = blend_file.MapPointerToBlock(attribute.data);
                const char* attribute_array_data_type = blend_file.TypeNameOfDataBlock(attribute_array_data);
                AttributeArray attribute_array = ReadDataBlock<AttributeArray>(blend_file, attribute_array_data, 0);

                std::cout << "    Data Type: " << attribute_array_data_type << " {\n";
                std::cout << "        Size: " << attribute_array.size << "\n";
                
                // Read raw attribute data bytes
                DataBlockNode* raw_data_block = blend_file.MapPointerToBlock(attribute_array.data);
                const char* raw_data_type = blend_file.TypeNameOfDataBlock(raw_data_block);
                raw_data* data = (raw_data*)blend_file.GetRawDataAddress(raw_data_block->data_offset);
                uint32_t byte_length = raw_data_block->block_header.byte_length;
                uint32_t number_structs = raw_data_block->block_header.struct_number;
                std::cout << "        Data Type: " << raw_data_type << "\n";
                std::cout << "        Byte Length: " << byte_length << "\n";
                std::cout << "        Number of Structs: " << number_structs << "\n";
                
                for (int array_idx = 0; array_idx < attribute_array.size; array_idx++) {
                    switch ((AttrType)attribute.data_type) {
                        case AttrType::Float3: {
                            float x, y, z;
                            int data_idx = array_idx * sizeof(float) * 3;
                            x = *reinterpret_cast<float*>(&data[data_idx]);
                            y = *reinterpret_cast<float*>(&data[data_idx + sizeof(float)]);
                            z = *reinterpret_cast<float*>(&data[data_idx + (sizeof(float) * 2)]);
                            
                            std::cout << "        {" << x << ", " << y << ", " << z << "}\n";
                            break;
                        }

                        case AttrType::Int32_2D: {
                            int32_t a, b;
                            int data_idx = array_idx * sizeof(int32_t) * 2;
                            a = *reinterpret_cast<int32_t*>(&data[data_idx]);
                            b = *reinterpret_cast<int32_t*>(&data[data_idx + sizeof(int32_t)]);

                            std::cout << "        {" << a << ", " << b << "}\n";
                            break;
                        }

                        case AttrType::Int32: {
                            int32_t i;
                            int data_idx = array_idx * sizeof(int32_t);
                            i = *reinterpret_cast<int32_t*>(&data[data_idx]);

                            std::cout << "        {" << i << "}\n";
                            break;
                        }
                        
                        case AttrType::Bool: {
                            bool b;
                            b = *reinterpret_cast<bool*>(&data[array_idx]);

                            if (b) {
                                std::cout << "        {true}\n";
                            } else {
                                std::cout << "        {false}\n";
                            }
                        }

                        default:
                            break;
                    }
                }


                std::cout << "    }\n";
            }
        }

        node = node->next;
    }

    InterpretDataBlocks(blend_file);

    return 0;
}