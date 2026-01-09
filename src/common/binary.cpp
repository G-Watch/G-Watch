#include <iostream>
#include <vector>

#include <libelf.h>
#include <gelf.h>
#include <string.h>

#include "common/common.hpp"
#include "common/log.hpp"
#include "common/binary.hpp"


GWBinaryImage::GWBinaryImage(){}


GWBinaryImage::~GWBinaryImage(){}


gw_retval_t GWBinaryImage::__verify_elf(Elf* elf){
    gw_retval_t retval = GW_SUCCESS;
    Elf_Kind ek;
    GElf_Ehdr ehdr;
    int elfclass;
    char *id;
    size_t program_header_num;
    size_t sections_num;
    size_t section_str_num;

    // verify ELF version
    if((ek=elf_kind(elf)) != ELF_K_ELF){ 
        GW_WARN("invalid ELF version: obtained(%d), expected(%d)", ek, ELF_K_ELF);
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    // obtain ELF header
    if(gelf_getehdr(elf, &ehdr) == NULL){
        GW_WARN("failed to obtain ELF header");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    // verify ELF class
    if ((elfclass = gelf_getclass(elf)) == ELFCLASSNONE){
        GW_WARN("invalid ELF class: obtained(%d)", elfclass);
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    // verify ELF identification data
    if((id = elf_getident(elf, NULL)) == NULL){
        GW_WARN("failed to obtain ELF identification data");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    // verify the number of section within the ELF
    if (elf_getshdrnum(elf, &sections_num) != 0){
        GW_WARN("no section found in ELF file");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    // verify the number of program header within the ELF
    if (elf_getphdrnum(elf, &program_header_num) != 0){
        GW_WARN("no program counter found in ELF file");
        // retval = GW_FAILED_INVALID_INPUT;
        // goto exit;
    }

    // verify the section index of the section header table within the ELF
    if(elf_getshdrstrndx(elf, &section_str_num) != 0){
        GW_WARN("no section header string table found in ELF file");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

exit:
    return retval;
}


gw_retval_t GWBinaryImage::__get_elf_section_by_name(Elf *elf, const char *name, Elf_Scn **section){
    gw_retval_t retval = GW_FAILED_NOT_EXIST;

    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    char *section_name = NULL;
    size_t str_section_index;

    GW_CHECK_POINTER(elf);
    GW_CHECK_POINTER(name);
    GW_CHECK_POINTER(section);

    // obtain the section index of the section header string table
    // this table stores all section names
    if (elf_getshdrstrndx(elf, &str_section_index) != 0){ 
        GW_WARN_DETAIL("failed to obtain the section index of the section header string table");
        retval = GW_FAILED_INVALID_INPUT;
        goto exit;
    }

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        if (gelf_getshdr(scn, &shdr) != &shdr) {
            GW_WARN_DETAIL("failed to obtain the section header");
            retval = GW_FAILED_INVALID_INPUT;
            goto exit;
        }
        if ((section_name = elf_strptr(elf, str_section_index, shdr.sh_name)) == NULL) {
            continue;
        }
        if (strcmp(section_name, name) == 0) {
            *section = scn;
            retval = GW_SUCCESS;
            break;
        }
    }

exit:
    return retval;
}


gw_retval_t GWBinaryImage::__get_elf_symtab(
    Elf *elf, Elf_Data **symbol_table_data, size_t *symbol_table_size, GElf_Shdr *symbol_table_shdr
){
    gw_retval_t retval = GW_SUCCESS;

    GElf_Shdr shdr;
    Elf_Scn *section = NULL;

    GW_CHECK_POINTER(elf);
    GW_CHECK_POINTER(symbol_table_data);
    GW_CHECK_POINTER(symbol_table_size); 

    // obtain the sysmbol table section
    GW_IF_FAILED(
        GWBinaryImage::__get_elf_section_by_name(elf, ".symtab", &section),
        retval,
        {
            GW_WARN_DETAIL("failed to obtain the symbol table section");
            goto exit;
        }
    );

    // obtain the section header
    if(gelf_getshdr(section, &shdr) == NULL){ 
        GW_WARN_DETAIL("failed to obtain the section header");
        retval = GW_FAILED;
        goto exit;
    }
    if (symbol_table_shdr != NULL) {
        *symbol_table_shdr = shdr;
    }

    // verify the section type
    if(shdr.sh_type != SHT_SYMTAB){
        GW_WARN_DETAIL("the section is not a symbol table");
        retval = GW_FAILED;
        goto exit;
    }

    // obtain the symbol table data
    if ((*symbol_table_data = elf_getdata(section, NULL)) == NULL) {
        GW_WARN_DETAIL("failed to obtain the symbol table data");
        retval = GW_FAILED;
        goto exit;
    }

    *symbol_table_size = shdr.sh_size / shdr.sh_entsize;

exit:
    return retval;
}


gw_retval_t GWBinaryImage::__estimate_elf_size(const uint8_t* data, uint64_t &size){
    gw_retval_t retval = GW_SUCCESS;
    bool is_64bit;
    uint64_t i, max_offset = 0, shdr_end  = 0, phdr_end = 0, segment_end = 0, section_end = 0;
    const Elf64_Ehdr* ehdr_64 = nullptr;
    const Elf64_Phdr* phdr_64 = nullptr;
    const Elf64_Shdr* shdr_64 = nullptr;
    const Elf32_Ehdr* ehdr_32 = nullptr;
    const Elf32_Phdr* phdr_32 = nullptr;
    const Elf32_Shdr* shdr_32 = nullptr;

    is_64bit = (data[EI_CLASS] == ELFCLASS64);

    if (is_64bit) {
        ehdr_64 = reinterpret_cast<const Elf64_Ehdr*>(data);
        if (ehdr_64->e_ident[EI_MAG0] != ELFMAG0 || 
            ehdr_64->e_ident[EI_MAG1] != ELFMAG1 || 
            ehdr_64->e_ident[EI_MAG2] != ELFMAG2 || 
            ehdr_64->e_ident[EI_MAG3] != ELFMAG3
        ){
            retval = GW_FAILED_INVALID_INPUT;
            goto exit;
        }

        max_offset = sizeof(Elf64_Ehdr);

        // program header table
        if (ehdr_64->e_phnum > 0) {
            phdr_end = ehdr_64->e_phoff + (ehdr_64->e_phnum * ehdr_64->e_phentsize);
            max_offset = std::max(max_offset, phdr_end);
    
            phdr_64 = reinterpret_cast<const Elf64_Phdr*>(data + ehdr_64->e_phoff);
            for (i = 0; i < ehdr_64->e_phnum; i++) {
                segment_end = phdr_64[i].p_offset + phdr_64[i].p_filesz;
                max_offset = std::max(max_offset, segment_end);
            }
        }
        
        // section header table
        if (ehdr_64->e_shnum > 0) {
            shdr_end = ehdr_64->e_shoff + (ehdr_64->e_shnum * ehdr_64->e_shentsize);
            max_offset = std::max(max_offset, shdr_end);
            
            shdr_64 = reinterpret_cast<const Elf64_Shdr*>(data + ehdr_64->e_shoff);
            for (i = 0; i < ehdr_64->e_shnum; i++) {
                if (shdr_64[i].sh_type != SHT_NOBITS) {
                    section_end = shdr_64[i].sh_offset + shdr_64[i].sh_size;
                    max_offset = std::max(max_offset, section_end);
                }
            }
        }
    } else {
        ehdr_32 = reinterpret_cast<const Elf32_Ehdr*>(data);

        if (ehdr_32->e_ident[EI_MAG0] != ELFMAG0 || 
            ehdr_32->e_ident[EI_MAG1] != ELFMAG1 || 
            ehdr_32->e_ident[EI_MAG2] != ELFMAG2 || 
            ehdr_32->e_ident[EI_MAG3] != ELFMAG3
        ){
            retval = GW_FAILED_INVALID_INPUT;
            goto exit;    
        }
        
        max_offset = sizeof(Elf32_Ehdr);
        
        // program header table
        if (ehdr_32->e_phnum > 0) {
            phdr_end = ehdr_32->e_phoff + (ehdr_32->e_phnum * ehdr_32->e_phentsize);
            max_offset = std::max(max_offset, phdr_end);

            phdr_32 = reinterpret_cast<const Elf32_Phdr*>(data + ehdr_32->e_phoff);
            for (i = 0; i < ehdr_32->e_phnum; i++) {
                size_t segment_end = phdr_32[i].p_offset + phdr_32[i].p_filesz;
                max_offset = std::max(max_offset, segment_end);
            }
        }
        
        // section header table
        if (ehdr_32->e_shnum > 0) {
            shdr_end = ehdr_32->e_shoff + (ehdr_32->e_shnum * ehdr_32->e_shentsize);
            max_offset = std::max(max_offset, shdr_end);

            shdr_32 = reinterpret_cast<const Elf32_Shdr*>(data + ehdr_32->e_shoff);
            for (i = 0; i < ehdr_32->e_shnum; i++) {
                if (shdr_32[i].sh_type != SHT_NOBITS) {
                    size_t section_end = shdr_32[i].sh_offset + shdr_32[i].sh_size;
                    max_offset = std::max(max_offset, section_end);
                }
            }
        }
    }

    size = max_offset;
    
exit:
    return retval;
}


size_t GWBinaryImage::__decompress_lz4(const uint8_t* input, size_t input_size, uint8_t* output, size_t output_size){
    size_t ipos = 0, opos = 0;  
    uint64_t next_nclen;  // length of next non-compressed segment
    uint64_t next_clen;   // length of next compressed segment
    uint64_t back_offset; // negative offset where redudant data is located, relative to current opos

    while (ipos < input_size) {
        next_nclen = (input[ipos] & 0xf0) >> 4;
        next_clen = 4 + (input[ipos] & 0xf);
        if (next_nclen == 0xf) {
            do {
                next_nclen += input[++ipos];
            } while (input[ipos] == 0xff);
        }
        
        memcpy(output + opos, input + (++ipos), next_nclen);

        ipos += next_nclen;
        opos += next_nclen;
        if (ipos >= input_size || opos >= output_size) {
            break;
        }
        back_offset = input[ipos] + (input[ipos + 1] << 8);       
        ipos += 2;
        if (next_clen == 0xf+4) {
            do {
                next_clen += input[ipos++];
            } while (input[ipos - 1] == 0xff);
        }

        if (next_clen <= back_offset) {
            memcpy(output + opos, output + opos - back_offset, next_clen);
        } else {
            memcpy(output + opos, output + opos - back_offset, back_offset);
            for (size_t i = back_offset; i < next_clen; i++) {
                output[opos + i] = output[opos + i - back_offset];
            }
        }

        opos += next_clen;
    }

    return opos;
}
