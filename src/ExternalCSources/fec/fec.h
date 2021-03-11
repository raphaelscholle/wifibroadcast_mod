#ifndef FEC_2_H
#define FEC_2_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct fec_parms *fec_code_t;
typedef unsigned char gf;

/*
 * create a new encoder, returning a descriptor. This contains k,n and
 * the encoding matrix.
 * n is the number of data blocks + fec blocks (matrix height)
 * k is just the data blocks (matrix width)
 */
void fec_init(void);

void fec_encode(unsigned int blockSize,
                const gf **data_blocks,
                unsigned int nrDataBlocks,
                gf **fec_blocks,
                unsigned int nrFecBlocks);

/** Documentation comes from https://github.com/DroneBridge/DroneBridge/blob/55eec5fad91a6faaaf6ac1fdd350d4db21a0435f/video/fec.c
* @param blockSize Size of packets
* @param data_blocks pointer to list of data packets
* @param nr_data_blocks number of data packets
* @param fec_blocks pointer to list of FEC packets
* @param fec_block_nos Indices of FEC packets that shall repair erased data packets in data packet list [array]
* @param erased_blocks Indices of erased data packets in FEC packet data list [array]
* @param nr_fec_blocks Number of FEC blocks used to repair data packets
*/
void fec_decode(unsigned int blockSize,
                gf **data_blocks,
                unsigned int nr_data_blocks,
                gf **fec_blocks,
                const unsigned int fec_block_nos[],
                const unsigned int erased_blocks[],
                unsigned short nr_fec_blocks  /* how many blocks per stripe */);

void fec_print(fec_code_t code, int width);

void fec_license(void);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <vector>
#include <array>


template<std::size_t S>
void fecEncode(unsigned int blockSize,std::vector<std::array<uint8_t,S>>& blockBuffer,unsigned int nPrimaryFragments,unsigned int nSecondaryFragments){
    assert(blockBuffer.size()>=nPrimaryFragments+nSecondaryFragments);
    assert(blockSize<=S);
    std::vector<uint8_t*> primaryFragments(nPrimaryFragments);
    std::vector<uint8_t*> secondaryFragments(nSecondaryFragments);
    for(int i=0;i<nPrimaryFragments;i++){
        primaryFragments[i]=blockBuffer[i].data();
    }
    for(int i=0;i<nSecondaryFragments;i++){
        secondaryFragments[i]=blockBuffer[nPrimaryFragments+i].data();
    }
    fec_encode(blockSize, (const unsigned char**)primaryFragments.data(),primaryFragments.size(), (unsigned char**)secondaryFragments.data(), secondaryFragments.size());
}

template<std::size_t S>
void fecDecode(unsigned int blockSize,std::vector<std::array<uint8_t,S>>& blockBuffer,unsigned int nPrimaryFragments,
               const std::vector<unsigned int>& indicesMissingPrimaryFragments,const std::vector<unsigned int>& indicesAvailableSecondaryFragments){
    // first validate input.
    assert(blockSize<=S);
    assert(indicesMissingPrimaryFragments.size()>=indicesAvailableSecondaryFragments.size());
    // I treat calling fecDecode() with more primary fragments than needed for the reconstruction step as an error here
    // (because it would create unneeded latency) though it would work just fine
    assert(indicesMissingPrimaryFragments.size()==indicesAvailableSecondaryFragments.size());
    // n of all theoretically possible locations for secondary fragments (could be optimized)
    const auto nTheoreticalSecondaryFragments=blockBuffer.size()-nPrimaryFragments;
    std::vector<uint8_t*> primaryFragments(nPrimaryFragments);
    std::vector<uint8_t*> secondaryFragments(nTheoreticalSecondaryFragments);
    for(int i=0;i<nPrimaryFragments;i++){
        primaryFragments[i]=blockBuffer[i].data();
    }
    for(int i=0;i<nTheoreticalSecondaryFragments;i++){
        secondaryFragments[i]=blockBuffer[nPrimaryFragments+i].data();
    }
    fec_decode(blockSize, primaryFragments.data(), nPrimaryFragments, secondaryFragments.data(), indicesAvailableSecondaryFragments.data(), indicesMissingPrimaryFragments.data(), indicesAvailableSecondaryFragments.size());
}

/**
 * @param blockSize Size of each FEC block,
 * @param primaryFragments list of pointers to raw data chunks (read and write). Mark missing primary fragments with nullptr.
 * @param secondaryFragments list of pointers to raw data chunks (read only). Mark missing secondary fragments with nullptr.
 */
//void fecDecode(unsigned int blockSize,const std::vector<uint8_t*>& primaryFragments,const std::vector<uint8_t*>& secondaryFragments){
//}

#endif

#endif //FEC_2_H

