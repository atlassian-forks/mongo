/**
 *    Copyright (C) 2021-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#pragma once

#include "mongo/db/exec/sbe/stages/stages.h"
#include "mongo/db/hasher.h"
#include "mongo/db/operation_context.h"
#include "mongo/db/query/plan_cache.h"
#include "mongo/db/query/sbe_stage_builder.h"
#include "mongo/db/service_context.h"

namespace mongo {
namespace sbe {

struct PlanCachePartitioner {
    // Determines the partitioning function for use with the 'Partitioned' utility.
    std::size_t operator()(const PlanCacheKey& k, const std::size_t nPartitions) const {
        return PlanCacheKeyHasher{}(k) % nPartitions;
    }
};

/**
 * Represents the data cached in the SBE plan cache. This data holds an execution plan and necessary
 * auxiliary data for preparing and executing the PlanStage tree.
 */
struct CachedSbePlan {
    CachedSbePlan(std::unique_ptr<sbe::PlanStage> root, stage_builder::PlanStageData data)
        : root(std::move(root)), planStageData(std::move(data)) {}

    std::unique_ptr<CachedSbePlan> clone() const {
        return std::make_unique<CachedSbePlan>(root->clone(), planStageData);
    }

    uint64_t estimateObjectSizeInBytes() const {
        return root->estimateCompileTimeSize();
    }

    std::unique_ptr<sbe::PlanStage> root;
    stage_builder::PlanStageData planStageData;
};

using PlanCacheEntry = PlanCacheEntryBase<CachedSbePlan>;

struct BudgetEstimator {
    size_t operator()(const PlanCacheEntry& entry) {
        return entry.estimatedEntrySizeBytes;
    }
};

using PlanCache = PlanCacheBase<PlanCacheKey,
                                CachedSbePlan,
                                BudgetEstimator,
                                PlanCachePartitioner,
                                PlanCacheKeyHasher>;

/**
 * A helper method to get the global SBE plan cache decorated in 'serviceCtx'.
 */
PlanCache& getPlanCache(ServiceContext* serviceCtx);

/**
 * A wrapper for the helper above. 'opCtx' cannot be null.
 */
PlanCache& getPlanCache(OperationContext* opCtx);

}  // namespace sbe
}  // namespace mongo
