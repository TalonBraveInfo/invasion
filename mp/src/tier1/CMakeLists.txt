project(tier1)

add_library(tier1 STATIC
        bitbuf.cpp
        newbitbuf.cpp
        byteswap.cpp
        characterset.cpp
        checksum_crc.cpp
        checksum_md5.cpp
        checksum_sha1.cpp
        commandbuffer.cpp
        convar.cpp
        datamanager.cpp
        diff.cpp
        generichash.cpp
        ilocalize.cpp
        interface.cpp
        KeyValues.cpp
        kvpacker.cpp
        lzmaDecoder.cpp
        mempool.cpp
        memstack.cpp
        NetAdr.cpp
        splitstring.cpp
        processor_detect.cpp
        rangecheckedvar.cpp
        reliabletimer.cpp
        stringpool.cpp
        strtools.cpp
        strtools_unicode.cpp
        tier1.cpp
        tokenreader.cpp
        sparsematrix.cpp
        uniqueid.cpp
        utlbuffer.cpp
        utlbufferutil.cpp
        utlstring.cpp
        utlsymbol.cpp
        utlbinaryblock.cpp
        snappy.cpp
        snappy-sinksource.cpp
        snappy-stubs-internal.cpp

        ../utils/lzma/C/LzmaDec.c)

target_include_directories(tier1 PRIVATE
        ../utils/common/
        ../public/tier0/
        ../public/tier1/
        )

target_compile_definitions(tier1 PRIVATE TIER1_STATIC_LIB)
target_link_libraries(tier1 PRIVATE Rpcrt4)
