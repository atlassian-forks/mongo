/**
 * Basic js tests for applying the collMod command converting regular indexes to unique indexes.
 *
 * @tags: [
 *   # applyOps is not supported on mongos
 *   assumes_against_mongod_not_mongos,
 *   # Cannot implicitly shard accessed collections because of collection existing when none
 *   # expected.
 *   assumes_no_implicit_collection_creation_after_drop,  # common tag in collMod tests.
 *   requires_fcv_52,
 *   requires_non_retryable_commands, # common tag in collMod tests.
 *   # applyOps uses the oplog that require replication support
 *   requires_replication,
 *   # TODO(SERVER-61181): Fix validation errors under ephemeralForTest.
 *   incompatible_with_eft,
 *   # TODO(SERVER-61182): Fix WiredTigerKVEngine::alterIdentMetadata() under inMemory.
 *   requires_persistence,
 * ]
 */

(function() {
'use strict';

const collModIndexUniqueEnabled = assert
                                      .commandWorked(db.getMongo().adminCommand(
                                          {getParameter: 1, featureFlagCollModIndexUnique: 1}))
                                      .featureFlagCollModIndexUnique.value;

if (!collModIndexUniqueEnabled) {
    jsTestLog('Skipping test because the collMod unique index feature flag is disabled.');
    return;
}

const collName = 'collmod_convert_to_unique_apply_ops';
const coll = db.getCollection(collName);
coll.drop();
assert.commandWorked(db.createCollection(collName));

function countUnique(key) {
    const all = coll.getIndexes().filter(function(z) {
        return z.unique && friendlyEqual(z.key, key);
    });
    return all.length;
}

// Creates a regular index and use collMod to convert it to a unique index.
assert.commandWorked(coll.createIndex({a: 1}));

// applyOps version of the following collMod command to convert index to unique:
//     {collMod: collName, index: {keyPattern: {a: 1}, unique: true}}
const applyOpsCmd = {
    applyOps: [
        {
            op: 'c',
            ns: db.$cmd.getFullName(),
            o: {
                collMod: coll.getName(),
                index: {
                    keyPattern: {a: 1},
                    unique: true,
                },
            },
        },
    ]
};

assert.commandWorked(coll.insert({_id: 1, a: 100}));
assert.commandWorked(coll.insert({_id: 2, a: 100}));
assert.commandWorked(coll.remove({_id: 2}));

// Successfully converts to a unique index.
const result = assert.commandWorked(db.adminCommand(applyOpsCmd));

// Check applyOps result.
assert.eq(1, result.applied, tojson(result));
assert.sameMembers([true], result.results, tojson(result));

// Look up index details in listIndexes output.
assert.eq(countUnique({a: 1}), 1, 'index should be unique now: ' + tojson(coll.getIndexes()));

// Test uniqueness constraint.
assert.commandFailedWithCode(coll.insert({_id: 100, a: 100}), ErrorCodes.DuplicateKey);
})();
