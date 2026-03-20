#include "temp_lib.hpp"

int main() {

    BlendFile blend_file = ReadBlendFile("Cube.blend");
    // ExtractSDNATypesToHeaderFile(blend_file);

    
    // LogBlendFileHeader(blend_file);
    
    // LogDataBlocks(blend_file);
    
    
    /* Attempt to read mesh data */
    /* See DNA_mesh_types.h struct Mesh */


    // DataBlockNode* node = blend_file.block_header_list.first;
    // while(node) {
    //     const DataBlockHeader& block_header = node->block_header;
    //     SDNA_Struct* struct_info = blend_file.sdna->structs[block_header.SDNA_type_index];
    //     const char* type_name = blend_file.sdna->types[struct_info->type_index];

    //     node = node->next;
    // }

    InterpretDataBlocks(blend_file);

    return 0;
}