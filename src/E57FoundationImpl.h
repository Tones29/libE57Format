#ifndef E57FOUNDATIONIMPL_H_INCLUDED
#define E57FOUNDATIONIMPL_H_INCLUDED

/*
 * E57FoundationImpl.h - private implementation header of E57 format reference implementation.
 *
 * Copyright 2009 - 2010 Kevin Ackley (kackley@gwi.net)
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <algorithm>
#include <set>
#include <stack>
#include <stdexcept>
#include <vector>

#include "Common.h"
#include "CheckedFile.h"

namespace e57 {

/// Forward declaration
template <typename RegisterT> class BitpackIntegerEncoder;
template <typename RegisterT> class BitpackIntegerDecoder;

class E57XmlParser;
class Decoder;
class Encoder;

//================================================================

class NodeImpl : public std::enable_shared_from_this<NodeImpl> {
public:
    virtual NodeType        type() const = 0;
    void                    checkImageFileOpen(const char* srcFileName, int srcLineNumber, const char* srcFunctionName) const;
    virtual bool            isTypeEquivalent(std::shared_ptr<NodeImpl> ni) = 0;
    bool                    isRoot() const;
    std::shared_ptr<NodeImpl> parent();
    ustring                 pathName() const;
    ustring                 relativePathName(std::shared_ptr<NodeImpl> origin, ustring childPathName = ustring()) const;
    ustring                 elementName() const;
    std::shared_ptr<ImageFileImpl> destImageFile();

    ustring                 imageFileName() const;
    virtual bool            isDefined(const ustring& pathName) = 0;
    bool                    isAttached() const;
    virtual void            setAttachedRecursive();

    void                    setParent(std::shared_ptr<NodeImpl> parent, const ustring& elementName);
    bool                    isTypeConstrained();

    virtual std::shared_ptr<NodeImpl> get(const ustring& pathName);
    virtual void            set(const ustring& pathName, std::shared_ptr<NodeImpl> ni, bool autoPathCreate = false);
    virtual void            set(const std::vector<ustring>& fields, unsigned level, std::shared_ptr<NodeImpl> ni, bool autoPathCreate = false);

    virtual void            checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) = 0;
    void                    checkBuffers(const std::vector<SourceDestBuffer>& sdbufs, bool allowMissing);
    bool                    findTerminalPosition(std::shared_ptr<NodeImpl> ni, uint64_t& countFromLeft);

    virtual void            writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) = 0;

    virtual                 ~NodeImpl() = default;

#ifdef E57_DEBUG
    virtual void            dump(int indent = 0, std::ostream& os = std::cout);
#endif

protected:
    friend class StructureNodeImpl;
    friend class CompressedVectorWriterImpl;
    friend class Decoder;
    friend class Encoder;

                                         NodeImpl(std::weak_ptr<ImageFileImpl> destImageFile);
    NodeImpl&                            operator=(NodeImpl& n);
    virtual std::shared_ptr<NodeImpl>  lookup(const ustring& /*pathName*/) {return(std::shared_ptr<NodeImpl>());}
    std::shared_ptr<NodeImpl>          getRoot();

    std::weak_ptr<ImageFileImpl>       destImageFile_;
    std::weak_ptr<NodeImpl>            parent_;
    ustring                            elementName_;
    bool                               isAttached_;
};

class StructureNodeImpl : public NodeImpl {
public:
                        StructureNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile);
    virtual             ~StructureNodeImpl()  override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;
    virtual void        setAttachedRecursive() override;

    virtual int64_t     childCount() const;
    virtual std::shared_ptr<NodeImpl> get(int64_t index);
    virtual std::shared_ptr<NodeImpl> get(const ustring& pathName) override;
    virtual void        set(int64_t index, std::shared_ptr<NodeImpl> ni);
    virtual void        set(const ustring& pathName, std::shared_ptr<NodeImpl> ni, bool autoPathCreate = false) override;
    virtual void        set(const std::vector<ustring>& fields, unsigned level, std::shared_ptr<NodeImpl> ni, bool autoPathCreate = false) override;
    virtual void        append(std::shared_ptr<NodeImpl> ni);

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    friend class CompressedVectorReaderImpl;
    virtual std::shared_ptr<NodeImpl> lookup(const ustring& pathName) override;

    std::vector<std::shared_ptr<NodeImpl> > children_;
};

class VectorNodeImpl : public StructureNodeImpl {
public:
    explicit            VectorNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile, bool allowHeteroChildren);
    virtual             ~VectorNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    bool                allowHeteroChildren() const;

    virtual void        set(int64_t index, std::shared_ptr<NodeImpl> ni) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    bool allowHeteroChildren_;
};

class SourceDestBufferImpl : public std::enable_shared_from_this<SourceDestBufferImpl> {
public:
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, int8_t* b,   const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(int8_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, uint8_t* b,  const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(uint8_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, int16_t* b,  const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(int16_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, uint16_t* b, const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(uint16_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, int32_t* b,  const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(int32_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, uint32_t* b, const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(uint32_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, int64_t* b,  const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(int64_t));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, bool* b,     const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(bool));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, float* b,    const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(float));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, double* b,   const size_t capacity, bool doConversion = false,
                         bool doScaling = false, size_t stride = sizeof(double));
    SourceDestBufferImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring pathName, std::vector<ustring>* b);

    ustring                 pathName()      const { return pathName_; }
    MemoryRepresentation    memoryRepresentation() const { return memoryRepresentation_; }
    void*                   base()          const { return base_; }
    std::vector<ustring>*   ustrings()      const { return ustrings_; }
    bool                    doConversion()  const { return doConversion_; }
    bool                    doScaling()     const { return doScaling_; }
    size_t                  stride()        const { return stride_; }
    size_t                  capacity()      const { return capacity_; }
    unsigned                nextIndex()     const { return nextIndex_; }
    void                    rewind()        { nextIndex_= 0; }

    int64_t         getNextInt64();
    int64_t         getNextInt64(double scale, double offset);
    float           getNextFloat();
    double          getNextDouble();
    ustring         getNextString();
    void            setNextInt64(int64_t value);
    void            setNextInt64(int64_t value, double scale, double offset);
    void            setNextFloat(float value);
    void            setNextDouble(double value);
    void            setNextString(const ustring& value);

    void            checkCompatible(std::shared_ptr<SourceDestBufferImpl> newBuf) const;

#ifdef E57_DEBUG
    void            dump(int indent = 0, std::ostream& os = std::cout);
#endif

protected:
    friend class BitpackIntegerEncoder<uint8_t>;
    friend class BitpackIntegerEncoder<uint16_t>;
    friend class BitpackIntegerEncoder<uint32_t>;
    friend class BitpackIntegerEncoder<uint64_t>;
    friend class BitpackIntegerDecoder<uint8_t>;
    friend class BitpackIntegerDecoder<uint16_t>;
    friend class BitpackIntegerDecoder<uint32_t>;
    friend class BitpackIntegerDecoder<uint64_t>;

    void                    checkState_() const;  /// Common routine to check that constructor arguments were ok, throws if not

    //??? verify alignment
    std::weak_ptr<ImageFileImpl> destImageFile_;
    ustring                 pathName_;      /// Pathname from CompressedVectorNode to source/dest object, e.g. "Indices/0"
    MemoryRepresentation    memoryRepresentation_;    /// Type of element (e.g. E57_INT8, E57_UINT64, DOUBLE...)
    char*                   base_;          /// Address of first element, for non-ustring buffers
    size_t                  capacity_;      /// Total number of elements in array
    bool                    doConversion_;  /// Convert memory representation to/from disk representation
    bool                    doScaling_;     /// Apply scale factor for integer type
    size_t                  stride_;        /// Distance between each element (different than size_ if elements not contiguous)
    unsigned                nextIndex_;     /// Number of elements that have been set (dest buffer) or read (source buffer) since rewind().
    std::vector<ustring>*   ustrings_;      /// Optional array of ustrings (used if memoryRepresentation_==E57_USTRING) ???ownership
};

//================================================================

class CompressedVectorNodeImpl : public NodeImpl {
public:
                        CompressedVectorNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile);
    virtual             ~CompressedVectorNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;
    virtual void        setAttachedRecursive() override;

    void                setPrototype(std::shared_ptr<NodeImpl> prototype);
    std::shared_ptr<NodeImpl> getPrototype();
    void                setCodecs(std::shared_ptr<VectorNodeImpl> codecs);
    std::shared_ptr<VectorNodeImpl> getCodecs();

    virtual int64_t     childCount();

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

    /// Iterator constructors
    std::shared_ptr<CompressedVectorWriterImpl> writer(std::vector<SourceDestBuffer> sbufs);
    std::shared_ptr<CompressedVectorReaderImpl> reader(std::vector<SourceDestBuffer> dbufs);

    int64_t             getRecordCount()                        {return(recordCount_);}
    uint64_t            getBinarySectionLogicalStart()          {return(binarySectionLogicalStart_);}
    void                setRecordCount(int64_t recordCount)    {recordCount_ = recordCount;}
    void                setBinarySectionLogicalStart(uint64_t binarySectionLogicalStart)
                                                                {binarySectionLogicalStart_ = binarySectionLogicalStart;}

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    friend class CompressedVectorReaderImpl;

    std::shared_ptr<NodeImpl> prototype_;
    std::shared_ptr<VectorNodeImpl> codecs_;

    int64_t                     recordCount_;
    uint64_t                    binarySectionLogicalStart_;
};

class IntegerNodeImpl : public NodeImpl {
public:
                        IntegerNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile, int64_t value = 0, int64_t minimum = 0, int64_t maximum = 0);
    virtual             ~IntegerNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;

    int64_t             value();
    int64_t             minimum();
    int64_t             maximum();

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    int64_t             value_;
    int64_t             minimum_;
    int64_t             maximum_;
};

class ScaledIntegerNodeImpl : public NodeImpl {
public:
                        ScaledIntegerNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile,
                            int64_t value = 0, int64_t minimum = 0, int64_t maximum = 0,
                            double scale = 1.0, double offset = 0.0);

                        ScaledIntegerNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile,
                            double scaledValue = 0., double scaledMinimum = 0., double scaledMaximum = 0.,
                            double scale = 1.0, double offset = 0.0);

    virtual             ~ScaledIntegerNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;

    int64_t             rawValue();
    double              scaledValue();
    int64_t             minimum();
    double              scaledMinimum();
    int64_t             maximum();
    double              scaledMaximum();
    double              scale();
    double              offset();

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;


#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    int64_t             value_;
    int64_t             minimum_;
    int64_t             maximum_;
    double              scale_;
    double              offset_;
};

class FloatNodeImpl : public NodeImpl {
public:
                        FloatNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile,
                                      double value = 0, FloatPrecision precision = E57_DOUBLE,
                                      double minimum = E57_DOUBLE_MIN, double  maximum = E57_DOUBLE_MAX);
    virtual             ~FloatNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;

    double              value();
    FloatPrecision      precision();
    double              minimum();
    double              maximum();

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    double              value_;
    FloatPrecision      precision_;
    double              minimum_;
    double              maximum_;
};

class StringNodeImpl : public NodeImpl {
public:
    explicit            StringNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile, const ustring value = "");
    virtual             ~StringNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;

    ustring             value();

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    ustring             value_;
};

class BlobNodeImpl : public NodeImpl {
public:
                        BlobNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile, int64_t byteCount);
                        BlobNodeImpl(std::weak_ptr<ImageFileImpl> destImageFile, int64_t fileOffset, int64_t length);
    virtual             ~BlobNodeImpl() override = default;

    virtual NodeType    type() const override;
    virtual bool        isTypeEquivalent(std::shared_ptr<NodeImpl> ni) override;
    virtual bool        isDefined(const ustring& pathName) override;

    int64_t             byteCount();
    void                read(uint8_t* buf, int64_t start, size_t count);
    void                write(uint8_t* buf, int64_t start, size_t count);

    virtual void        checkLeavesInSet(const std::set<ustring>& pathNames, std::shared_ptr<NodeImpl> origin) override;

    virtual void        writeXml(std::shared_ptr<ImageFileImpl> imf, CheckedFile& cf, int indent, const char* forcedFieldName=nullptr) override;

#ifdef E57_DEBUG
    void                dump(int indent = 0, std::ostream& os = std::cout) override;
#endif

protected:
    uint64_t            blobLogicalLength_;
    uint64_t            binarySectionLogicalStart_;
    uint64_t            binarySectionLogicalLength_;
};

/// Note: If any fields are added to this structure, swab() may need to be updated.
struct E57FileHeader {
    char        fileSignature[8];
    uint32_t    majorVersion;
    uint32_t    minorVersion;
    uint64_t    filePhysicalLength;
    uint64_t    xmlPhysicalOffset;
    uint64_t    xmlLogicalLength;
    uint64_t    pageSize;
//  char        e57LibraryVersion[8];   //Not in V1.0 Standard

#ifdef E57_BIGENDIAN
    void        swab();
#else
    void        swab(){}
#endif
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout);
#endif
};

class ImageFileImpl : public std::enable_shared_from_this<ImageFileImpl> {
public:
                    ImageFileImpl( ReadChecksumPolicy policy );
    void            construct2(const ustring& fileName, const ustring& mode);
    std::shared_ptr<StructureNodeImpl> root();
    void            close();
    void            cancel();
    bool            isOpen();
    bool            isWriter();
    int             writerCount();
    int             readerCount();
                    ~ImageFileImpl();

    uint64_t        allocateSpace(uint64_t byteCount, bool doExtendNow);
    CheckedFile*    file();
    ustring         fileName();

    /// Manipulate registered extensions in the file
    void            extensionsAdd(const ustring& prefix, const ustring& uri);
    bool            extensionsLookupPrefix(const ustring& prefix, ustring& uri);
    bool            extensionsLookupUri(const ustring& uri, ustring& prefix);
    size_t          extensionsCount();
    ustring         extensionsPrefix(const size_t index);
    ustring         extensionsUri(const size_t index);

    /// Utility functions:
    bool            isElementNameExtended(const ustring& elementName);
    bool            isElementNameLegal(const ustring& elementName, bool allowNumber = true);
    bool            isPathNameLegal(const ustring& pathName);
    void            checkElementNameLegal(const ustring& elementName, bool allowNumber = true);
    void            elementNameParse(const ustring& elementName, ustring& prefix, ustring& localPart, bool allowNumber = true);

    void            pathNameCheckWellFormed(const ustring& pathName);
    void            pathNameParse(const ustring& pathName, bool& isRelative, std::vector<ustring>& fields);
    ustring         pathNameUnparse(bool isRelative, const std::vector<ustring>& fields);

    unsigned        bitsNeeded(int64_t minimum, int64_t maximum);
    static void     readFileHeader(CheckedFile* file, E57FileHeader& header);
    void            incrWriterCount();
    void            decrWriterCount();
    void            incrReaderCount();
    void            decrReaderCount();

    /// Diagnostic functions:
#ifdef E57_DEBUG
    void            dump(int indent = 0, std::ostream& os = std::cout);
#endif

protected:
    friend class E57XmlParser;
    friend class BlobNodeImpl;
    friend class CompressedVectorWriterImpl;
    friend class CompressedVectorReaderImpl; //??? add file() instead of accessing file_, others friends too

    void checkImageFileOpen(const char* srcFileName, int srcLineNumber, const char* srcFunctionName);

    struct NameSpace {
        ustring     prefix;
        ustring     uri;
                    NameSpace(ustring prefix0, ustring uri0) : prefix(prefix0),uri(uri0) {}
    };

    ustring         fileName_;
    bool            isWriter_;
    int             writerCount_;
    int             readerCount_;

    ReadChecksumPolicy   checksumPolicy;

    CheckedFile*    file_;

    /// Read file attributes
    uint64_t        xmlLogicalOffset_;
    uint64_t        xmlLogicalLength_;

    /// Write file attributes
    uint64_t        unusedLogicalStart_;

    /// Bidirectional map from namespace prefix to uri
    std::vector<NameSpace>  nameSpaces_;

    /// Smart pointer to metadata tree
    std::shared_ptr<StructureNodeImpl> root_;
};

//================================================================


class SeekIndex {
public:
    ///!!! implement seek
#ifdef E57_DEBUG
    void        dump(int /*indent*/ = 0, std::ostream& /*os*/ = std::cout) {/*???*/}
#endif
};

//================================================================

struct CompressedVectorSectionHeader {
    uint8_t     sectionId;              // = E57_COMPRESSED_VECTOR_SECTION
    uint8_t     reserved1[7];           // must be zero
    uint64_t    sectionLogicalLength;   // byte length of whole section
    uint64_t    dataPhysicalOffset;     // offset of first data packet
    uint64_t    indexPhysicalOffset;    // offset of first index packet

                CompressedVectorSectionHeader();
    void        verify(uint64_t filePhysicalSize=0);
#ifdef E57_BIGENDIAN
    void        swab();
#else
    void        swab(){}
#endif
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout);
#endif
};

//================================================================

#define E57_DATA_PACKET_MAX (64*1024)  /// maximum size of CompressedVector binary data packet   ??? where put this


struct DataPacketHeader {  ///??? where put this
    uint8_t     packetType;         // = E57_DATA_PACKET
    uint8_t     packetFlags;
    uint16_t    packetLogicalLengthMinus1;
    uint16_t    bytestreamCount;

                DataPacketHeader();
    void        verify(unsigned bufferLength = 0) const; //???use
#ifdef E57_BIGENDIAN
    void        swab();
#else
    void        swab(){}
#endif
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout) const;
#endif
};

//================================================================

struct DataPacket {  /// Note this is full sized packet, not just header
    uint8_t     packetType;         // = E57_DATA_PACKET
    uint8_t     packetFlags;
    uint16_t    packetLogicalLengthMinus1;
    uint16_t    bytestreamCount;
    uint8_t     payload[64*1024-6]; // pad packet to full length, can't spec layout because depends bytestream data

                DataPacket();
    void        verify(unsigned bufferLength = 0) const;
    char*       getBytestream(unsigned bytestreamNumber, unsigned& bufferLength);
    unsigned    getBytestreamBufferLength(unsigned bytestreamNumber);

#ifdef E57_BIGENDIAN
    void        swab(bool toLittleEndian);    //??? change to swabIfBigEndian() and elsewhere
#else
    void        swab(bool /*toLittleEndian*/){}
#endif
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout) const;
#endif
};

//================================================================

struct EmptyPacketHeader {
    uint8_t     packetType;    // = E57_EMPTY_PACKET
    uint8_t     reserved1;     // must be zero
    uint16_t    packetLogicalLengthMinus1;

                EmptyPacketHeader();
    void        verify(unsigned bufferLength = 0) const; //???use
#ifdef E57_BIGENDIAN
    void        swab();
#else
    void        swab(){}
#endif
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout) const;
#endif
};

//================================================================

struct DecodeChannel {
    SourceDestBuffer    dbuf; //??? for now, one input per channel
    std::shared_ptr<Decoder> decoder;
    unsigned            bytestreamNumber;
    uint64_t            maxRecordCount;
    uint64_t            currentPacketLogicalOffset;
    size_t              currentBytestreamBufferIndex;
    size_t              currentBytestreamBufferLength;
    bool                inputFinished;

                        DecodeChannel(SourceDestBuffer dbuf_arg, std::shared_ptr<Decoder> decoder_arg, unsigned bytestreamNumber_arg, uint64_t maxRecordCount_arg);

    bool                isOutputBlocked() const;
    bool                isInputBlocked() const;   /// has exhausted data in the current packet
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout);
#endif
};

//================================================================

class PacketReadCache;

class CompressedVectorReaderImpl {
public:
                CompressedVectorReaderImpl(std::shared_ptr<CompressedVectorNodeImpl> ni, std::vector<SourceDestBuffer>& dbufs);
                ~CompressedVectorReaderImpl();
    unsigned    read();
    unsigned    read(std::vector<SourceDestBuffer>& dbufs);
    void        seek(uint64_t recordNumber);
    bool        isOpen() const;
    std::shared_ptr<CompressedVectorNodeImpl> compressedVectorNode() const;
    void        close();

#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout);
#endif

protected:
    void        checkImageFileOpen(const char* srcFileName, int srcLineNumber, const char* srcFunctionName);
    void        checkReaderOpen(const char* srcFileName, int srcLineNumber, const char* srcFunctionName) const;
    void        setBuffers(std::vector<SourceDestBuffer>& dbufs); //???needed?
    uint64_t    earliestPacketNeededForInput() const;
    void        feedPacketToDecoders(uint64_t currentPacketLogicalOffset);
    uint64_t    findNextDataPacket(uint64_t nextPacketLogicalOffset);

    //??? no default ctor, copy, assignment?

    bool                                        isOpen_;
    std::vector<SourceDestBuffer>               dbufs_;
    std::shared_ptr<CompressedVectorNodeImpl> cVector_;
    std::shared_ptr<NodeImpl>                 proto_;
    std::vector<DecodeChannel>                  channels_;
    PacketReadCache*                            cache_;

    uint64_t    recordCount_;                   /// number of records written so far
    uint64_t    maxRecordCount_;
    uint64_t    sectionEndLogicalOffset_;
};

//================================================================

class CompressedVectorWriterImpl {
public:
                CompressedVectorWriterImpl(std::shared_ptr<CompressedVectorNodeImpl> ni, std::vector<SourceDestBuffer>& sbufs);
                ~CompressedVectorWriterImpl();
    void        write(const size_t requestedRecordCount);
    void        write(std::vector<SourceDestBuffer>& sbufs, const size_t requestedRecordCount);
    bool        isOpen() const;
    std::shared_ptr<CompressedVectorNodeImpl> compressedVectorNode() const;
    void        close();

#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout);
#endif

protected:
    void        checkImageFileOpen(const char* srcFileName, int srcLineNumber, const char* srcFunctionName);
    void        checkWriterOpen(const char* srcFileName, int srcLineNumber, const char* srcFunctionName) const;
    void        setBuffers(std::vector<SourceDestBuffer>& sbufs); //???needed?
    size_t      totalOutputAvailable() const;
    size_t      currentPacketSize() const;
    uint64_t    packetWrite();
    void        flush();

    //??? no default ctor, copy, assignment?

    std::vector<SourceDestBuffer>               sbufs_;
    std::shared_ptr<CompressedVectorNodeImpl> cVector_;
    std::shared_ptr<NodeImpl>                 proto_;

    std::vector<std::shared_ptr<Encoder> >  bytestreams_;
    SeekIndex               seekIndex_;
    DataPacket              dataPacket_;

    bool                    isOpen_;
    uint64_t                sectionHeaderLogicalStart_;     /// start of CompressedVector binary section
    uint64_t                sectionLogicalLength_;          /// total length of CompressedVector binary section
    uint64_t                dataPhysicalOffset_;            /// start of first data packet
    uint64_t                topIndexPhysicalOffset_;        /// top level index packet
    uint64_t                recordCount_;                   /// number of records written so far
    uint64_t                dataPacketsCount_;              /// number of data packets written so far
    uint64_t                indexPacketsCount_;             /// number of index packets written so far
};

//================================================================

class PacketLock {
public:
                    ~PacketLock();

private:
    /// Can't be copied or assigned
                    PacketLock(const PacketLock& plock);
    PacketLock&     operator=(const PacketLock& plock);

protected:
    friend class PacketReadCache;
    /// Only PacketReadCache can construct
                     PacketLock(PacketReadCache* cache, unsigned cacheIndex);

    PacketReadCache* cache_;
    unsigned         cacheIndex_;
};

//================================================================

class PacketReadCache {
public:
                        PacketReadCache(CheckedFile* cFile, unsigned packetCount);
                        ~PacketReadCache();

    std::unique_ptr<PacketLock> lock(uint64_t packetLogicalOffset, char* &pkt);  //??? pkt could be const

#ifdef E57_DEBUG
      void                dump(int indent = 0, std::ostream& os = std::cout);
#endif
protected:
    /// Only PacketLock can unlock the cache
    friend class PacketLock;
    void                unlock(unsigned cacheIndex);

    void                readPacket(unsigned oldestEntry, uint64_t packetLogicalOffset);

    struct CacheEntry {
        uint64_t    logicalOffset_;
        char*       buffer_;  //??? could be const?
        unsigned    lastUsed_;
    };

    unsigned            lockCount_;
    unsigned            useCount_;
    CheckedFile*        cFile_;
    std::vector<CacheEntry>  entries_;
};

} /// end namespace e57

//================================================================
struct IndexPacket {  /// Note this is whole packet, not just header
    static const unsigned MAX_ENTRIES = 2048;

    uint8_t     packetType;     // = E57_INDEX_PACKET
    uint8_t     packetFlags;    // flag bitfields
    uint16_t    packetLogicalLengthMinus1;
    uint16_t    entryCount;
    uint8_t     indexLevel;
    uint8_t     reserved1[9];   // must be zero
    struct IndexPacketEntry {
        uint64_t    chunkRecordNumber;
        uint64_t    chunkPhysicalOffset;
    } entries[MAX_ENTRIES];

                IndexPacket();
    void        verify(unsigned bufferLength = 0, uint64_t totalRecordCount = 0, uint64_t fileSize = 0) const;
#ifdef E57_BIGENDIAN
    void        swab(bool toLittleEndian);
#else
    void        swab(bool /*toLittleEndian*/) {}
#endif
#ifdef E57_DEBUG
    void        dump(int indent = 0, std::ostream& os = std::cout) const;
#endif
};

#endif // E57FOUNDATIONIMPL_H_INCLUDED
