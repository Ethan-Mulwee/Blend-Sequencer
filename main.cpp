#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <exception>


// Aka BHead or LargeBHead8 in blender source
struct BlockHeader {
    int32_t code;
    int32_t SDNAnr;
    uint64_t old_pointer;
    int64_t len;
    int64_t nr;
};

// Aka BHeadN
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

struct BlendFile {
    char* header;
    int header_length;
    int format_version;
    int blender_version;

    BlockHeaderList block_header_list;
};



BlendFile ReadBlendFile(const char* path) {
    BlendFile result;

    std::ifstream file(path, std::ios::in | std::ios::binary);

    /* ------------------------------- READ HEADER ------------------------------ */
    // hardcoded because this isn't really expected to change
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
        int file_offset = file.tellg();
        file.read((char*)&block_header, sizeof(BlockHeader));
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

    std::cout << "\nblock:\n";
    BlockHeader& block_header = blendFile.block_header_list.first->block_header;
    char code[sizeof(int32_t) + 1];
    code[sizeof(int32_t)] = '\0';
    Int32ToChar(code, block_header.code);
    std::cout << "code: " << code << "\n";
    std::cout << "SDNA struct type: " << block_header.SDNAnr << "\n";
    std::cout << "old pointer: " << block_header.old_pointer << "\n";
    std::cout << "byte length: " << block_header.len << "\n";
    std::cout << "number of structs: " << block_header.nr << "\n";

    std::cout << "\nblock:\n";
    BlockHeader& block_header2 = blendFile.block_header_list.first->next->block_header;
    char code2[sizeof(int32_t) + 1];
    code[sizeof(int32_t)] = '\0';
    Int32ToChar(code2, block_header2.code);
    std::cout << "code: " << code2 << "\n";
    std::cout << "SDNA struct type: " << block_header2.SDNAnr << "\n";
    std::cout << "old pointer: " << block_header2.old_pointer << "\n";
    std::cout << "byte length: " << block_header2.len << "\n";
    std::cout << "number of structs: " << block_header2.nr << "\n";
}