// File: gw2formats/pf/PackFile.h

#pragma once

#ifndef GW2FORMATS_PF_PACKFILE_H_INCLUDED
#define GW2FORMATS_PF_PACKFILE_H_INCLUDED

#include <fstream>
#include <memory>
#include <vector>

#include <gw2formats/base.h>
#include <gw2formats/pf/ChunkFactory.h>

//
// The reason this is a template is cause ChunkFactory requires the file's type
// to be available at compile time (since some files share chunk ids).
//

namespace gw2f {
namespace pf {

/** Opens and handles a Guild Wars 2 PackFile.
 *  \tparam TFileType   The fourcc of the expected pf format. */
template <uint32 TFileType>
    class PackFile
{

#pragma pack(push, 1)

    struct FileHeader
    {
        uint8 magic[2];
        uint16 descriptorType;
        uint16 zero;
        uint16 headerSize;
        uint32 contentType;
    };

    struct ChunkHeader
    {
        uint32 magic;
        uint32 nextChunkOffset;
        uint16 version;
        uint16 headerSize;
        uint32 descriptorOffset;
    };

#pragma pack(pop)

private:
    typedef std::vector<byte> DataVector;

private:
    std::shared_ptr<DataVector> m_data;
    const FileHeader*           m_header;

public:
    /** Default constructor. Constructs an empty PackFile. */
    PackFile()
        : m_data(new DataVector)
        , m_header(nullptr)
    {
    }

    /** Creates the PackFile and loads its data from file.
     *  \param[in]  p_filename  Filename to load. */
    PackFile(const char* p_filename)
        : m_header(nullptr)
    {
        load(p_filename);
    }

    /** Creates the PackFile and assigns it the data contained in p_data.
     *  \param[in]  p_data  Data containing PackFile data.
     *  \param[in]  p_size  Size of p_data. */
    PackFile(const byte* p_data, uint32 p_size)
        : m_header(nullptr)
    {
        assign(p_data, p_size);
    }

    /** Copy constructor. Clones the data of the given PackFile.
     *  \param[in]  p_other     PackFile to copy. */
    PackFile(const PackFile& p_other)
        : m_data(p_other.m_data)
        , m_header(nullptr)
    {
        if (m_data->size()) {
            m_header = reinterpret_cast<const FileHeader*>(m_data->data());
        }
    }

    /** Destructor. */
    ~PackFile()
    {
    }

    /** Copies the data contained in p_other to this object. */
    PackFile& operator=(const PackFile& p_other)
    {
        m_data = p_other.m_data;

        if (m_data->size()) {
            m_header = reinterpret_cast<const FileHeader*>(m_data->data());
        } else {
            m_header = nullptr;
        }

        return *this;
    }

    /** Loads this packfile's data from the given file.
     *  \param[in]  p_filename  Filename to load.
     *  \return     bool        True if successful, false if not. */
    bool load(const char* p_filename)
    {
        std::ifstream input(p_filename, std::ios::in | std::ios::binary);
        if (!input.is_open()) { return false; }

        input.seekg(0, std::ios::end);
        uint32 size = static_cast<uint32>(input.tellg());
        input.seekg(0, std::ios::beg);

        std::vector<byte> data(size);
        input.read(reinterpret_cast<char*>(data.data()), data.size());
        input.close();

        return assign(data.data(), data.size());
    }

    /** Assigns this PackFile the contents of the given data.
     *  \param[in]  p_data      Data to assign.
     *  \param[in]  p_size      Size of p_data.
     *  \return     bool        True if successful, false if not. */
    bool assign(const byte* p_data, uint32 p_size)
    {
        if (!p_data || !p_size) { return false; }
        if (p_size < sizeof(FileHeader)) { return false; }

        auto header = reinterpret_cast<const FileHeader*>(p_data);

        if (header->magic[0] != 'P' || header->magic[1] != 'F') { return false; }
        if (header->contentType != TFileType) { return false; }

        m_data.reset(new DataVector(p_size));
        std::memcpy(m_data->data(), p_data, p_size);
        m_header = reinterpret_cast<const FileHeader*>(m_data->data());
        return true;
    }

    /** Gets the fourcc of the data contained in this PackFile, or zero if no
     *  data is loaded. */
    dword type() const
    {
        return m_header ? TFileType : 0;
    }

    /** Looks for a chunk with the given identifier and returns its data if
     *  found. This data is still owned by the PackFile and must be copied if
     *  it is to be modified.
     *  \param[in]  p_identifier    Chunk identifier.
     *  \param[out] po_size         Size of the returned data.
     *  \return     const byte*     Pointer to the chunk's data if found, or 
     *                              nullptr if not found. */
    const byte* chunk(dword p_identifier, uint32& po_size) const
    {
        po_size = 0;
        if (!m_header) { return nullptr; }

        auto pointer = m_data->data() + sizeof(*m_header);
        auto end     = m_data->data() + m_data->size();

        while (pointer < end) {
            auto remaining = (end - pointer);
            if (remaining < sizeof(ChunkHeader)) { break; }

            auto chunkHead = reinterpret_cast<const ChunkHeader*>(pointer);
            auto chunkSize = chunkHead->nextChunkOffset + offsetof(ChunkHeader, nextChunkOffset) + sizeof(chunkHead->nextChunkOffset);

            if (chunkHead->magic == p_identifier) {
                po_size = chunkSize - chunkHead->headerSize;
                return pointer + sizeof(*chunkHead);
            }

            pointer += chunkSize;
        }

        return nullptr;
    }

    /** Looks for a chunk with the given identifier and returns a structure
     *  containing its data.
     *  \tparam     TId                 Chunk identifier.
     *  \return     shared_ptr<struct>  Shared pointer containing a chunk-
     *                                  specific struct, with the found chunk's
     *                                  data. If none was found, the shared_ptr
     *                                  contains nullptr. */
    template <dword TId>
        std::shared_ptr<typename ChunkFactory<TFileType,TId>::Type> chunk() const
    {
        uint32 size;
        const byte* data = chunk(TId, size);
        return std::make_shared<typename ChunkFactory<TFileType,TId>::Type>(data, size);
    }
};

// FourCC names, in alphabetic order
typedef PackFile<fcc::AMAT> AmatPackFile;
typedef PackFile<fcc::MODL> ModlPackFile;

// Descriptive names, in alphabetic order
typedef PackFile<fcc::AMAT> MaterialPackFile;
typedef PackFile<fcc::MODL> ModelPackFile;

}; // namespace pf
}; // namespace gw2f

#endif // GW2FORMATS_PF_PACKFILE_H_INCLUDED