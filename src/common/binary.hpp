#pragma once

#include <iostream>
#include <vector>
#include <fstream>

#include <libelf.h>
#include <gelf.h>
#include <string.h>

#include "common/common.hpp"
#include "common/log.hpp"


class GWBinaryImage {
    /* ==================== Common ==================== */
 public:
    /*!
     *  \brief  constructor
     */
    GWBinaryImage();


    /*!
     *  \brief  destructor
     */
    virtual ~GWBinaryImage();


    /*!
     *  \brief  fill in the binary data
     *  \param  data        pointer to the binary data
     *  \param  size        size of the binary data, if the size if 0,
     *                      then the binary data will be parsed based
     *                      on the given binary data
     *  \return GW_SUCCESS for successfully fill
     */
    virtual gw_retval_t fill(const void *data, uint64_t size){
        gw_retval_t retval = GW_SUCCESS;

        GW_CHECK_POINTER(data);
        GW_ASSERT(size > 0);

        this->_binary.resize(size);
        memcpy(this->_binary.data(), data, size);

    exit:
        return retval;
    };


    /*!
     *  \brief  fill in the binary data from a file
     *  \param  path        path to the binary file
     *  \return GW_SUCCESS for successfully fill
     */
    virtual gw_retval_t fill(const std::string &path){
        gw_retval_t retval = GW_SUCCESS;
        std::ifstream binary_file(path, std::ios::binary | std::ios::ate);

        GW_ASSERT(binary_file.is_open());

        std::streamsize size = binary_file.tellg();
        binary_file.seekg(0, std::ios::beg);

        this->_binary.resize(size);
        binary_file.read((char*)this->_binary.data(), size);

        binary_file.close();

    exit:
        return retval;
    };


    /*!
     *  \brief  parse the binary image
     *  \note   this function should be called after calling fill
     *  \return GW_SUCCESS for successfully parse
     */
    virtual gw_retval_t parse(){
        return GW_FAILED_NOT_IMPLEMENTAED;
    }


    /*!
     *  \brief  dump the binary image to specified path
     *  \param  dump_path   path to dump the parsed bianry data
     */
    virtual void dump(std::string dump_path){
        std::ofstream dump_file;
        dump_file.open(dump_path, std::ios::out | std::ios::binary);
        dump_file.write((char*)this->_binary.data(), this->_binary.size());
        dump_file.close();
    }


    /*!
     *  \brief  obtain the read-only pointer to the data within the binary
     *  \return pointer to the data within the binary 
     */
    inline const uint8_t* data(){ return this->_binary.data(); };


    /*!
     *  \brief  obtain the size of the binary
     *  \return size of the binary
     */
    inline uint64_t size(){ return this->_binary.size(); };


 protected:
    // binary data
    std::vector<uint8_t> _binary;

    // whether the binary has been parsed
    bool _is_parsed = false;
    /* ==================== Common ==================== */


    /* ==================== ELF ==================== */
 protected:
    /*!
     *  \brief  verify whether a given binary is a valid ELF binary
     *  \param  elf     pointer to the (potential) ELF structure
     *  \return GW_SUCCESS for valid ELF binary
     */
    static gw_retval_t __verify_elf(Elf* elf);


    /*!
     *  \brief  obtain the ELF section by name
     *  \param  elf     pointer to the ELF structure
     *  \param  name    name of the section
     *  \param  section pointer to the obtained section
     *  \return GW_SUCCESS for successfully obtain the section
     */
    static gw_retval_t __get_elf_section_by_name(Elf *elf, const char *name, Elf_Scn **section);


    /*!
     *  \brief  obtain the ELF section by name
     *  \param  elf                 pointer to the ELF structure
     *  \param  symbol_table_data   pointer to the obtained symbol table
     *  \param  symbol_table_size   pointer to the size of the obtained symbol table
     *  \param  symbol_table_shdr   pointer to the obtained symbol table header
     *  \return GW_SUCCESS for successfully obtain the section
     */
    static gw_retval_t __get_elf_symtab(
        Elf *elf, Elf_Data **symbol_table_data, size_t *symbol_table_size, GElf_Shdr *symbol_table_shdr
    );


    /*!
     *  \brief  estimate the size of ELF file
     *  \param  data     pointer to the (potential) ELF structure
     *  \param  size     estimated size
     *  \return GW_SUCCESS for valid estimation
     */
    static gw_retval_t __estimate_elf_size(const uint8_t* data, uint64_t &size);
    /* ==================== ELF ==================== */


    /* ==================== Decompress ==================== */
    /*!
     *  \brief  decompress a input area based on LZ4 algorithm
     *  \param  input       pointer to the input data
     *  \param  output      pointer to the output data
     *  \param  output_size size of the output data
     *  \return size of the decompressed data
     */
    static size_t __decompress_lz4(
        const uint8_t* input,
        size_t input_size,
        uint8_t* output,
        size_t output_size
    );
    /* ==================== Decompress ==================== */
};
