//
// Created by ameya on 2/20/2025.
//

#include "BitmapManager.h"

BitmapManager::BitmapManager(const block_index_t startBlock, const block_index_t numBlocks, const block_index_t size,
                             BlockManager* blockManager): startBlock(startBlock), numBlocks(numBlocks), size(size),
                                                          blockManager(blockManager)
{
}
