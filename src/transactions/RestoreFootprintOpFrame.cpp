// Copyright 2023 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
#include "transactions/RestoreFootprintOpFrame.h"

namespace stellar
{

struct RestoreFootprintMetrics
{
    medida::MetricsRegistry& mMetrics;

    size_t mLedgerReadByte{0};
    size_t mLedgerWriteByte{0};

    RestoreFootprintMetrics(medida::MetricsRegistry& metrics)
        : mMetrics(metrics)
    {
    }

    ~RestoreFootprintMetrics()
    {
        mMetrics
            .NewMeter({"soroban", "restore-fprint-op", "read-ledger-byte"},
                      "byte")
            .Mark(mLedgerReadByte);
        mMetrics
            .NewMeter({"soroban", "restore-fprint-op", "write-ledger-byte"},
                      "byte")
            .Mark(mLedgerWriteByte);
    }
};

RestoreFootprintOpFrame::RestoreFootprintOpFrame(Operation const& op,
                                                 OperationResult& res,
                                                 TransactionFrame& parentTx)
    : OperationFrame(op, res, parentTx)
    , mRestoreFootprintOp(mOperation.body.restoreFootprintOp())
{
}

bool
RestoreFootprintOpFrame::isOpSupported(LedgerHeader const& header) const
{
    return header.ledgerVersion >= 20;
}

bool
RestoreFootprintOpFrame::doApply(AbstractLedgerTxn& ltx)
{
    throw std::runtime_error("RestoreFootprintOpFrame::doApply needs Config");
}

bool
RestoreFootprintOpFrame::doApply(Application& app, AbstractLedgerTxn& ltx,
                                 Hash const& sorobanBasePrngSeed)
{
    RestoreFootprintMetrics metrics(app.getMetrics());

    auto const& resources = mParentTx.sorobanResources();
    auto const& footprint = resources.footprint;
    auto ledgerSeq = ltx.loadHeader().current().ledgerSeq;

    auto const& expirationSettings = app.getLedgerManager()
                                         .getSorobanNetworkConfig(ltx)
                                         .stateExpirationSettings();
    rust::Vec<CxxLedgerEntryRentChange> rustEntryRentChanges;
    // Bump the rent on the restored entry to minimum expiration, including
    // the current ledger.
    uint32_t restoredExpirationLedger =
        ledgerSeq + expirationSettings.minPersistentEntryExpiration - 1;
    for (auto const& lk : footprint.readWrite)
    {
        auto keySize = xdr::xdr_size(lk);
        auto ltxe = ltx.loadWithoutRecord(lk, /*loadExpiredEntry=*/true);
        if (!ltxe)
        {
            // Skip entries that don't exist.
            continue;
        }

        auto entrySize = xdr::xdr_size(ltxe.current());
        metrics.mLedgerReadByte += keySize + entrySize;
        if (resources.readBytes < metrics.mLedgerReadByte)
        {
            innerResult().code(RESTORE_FOOTPRINT_RESOURCE_LIMIT_EXCEEDED);
            return false;
        }

        if (isLive(ltxe.current(), ledgerSeq))
        {
            // Skip entries that are already live.
            continue;
        }
        LedgerEntry restoredEntry = ltxe.current();
        metrics.mLedgerWriteByte += keySize + entrySize;

        if (resources.extendedMetaDataSizeBytes <
                metrics.mLedgerWriteByte * 2 ||
            resources.writeBytes < metrics.mLedgerWriteByte ||
            resources.readBytes < metrics.mLedgerReadByte)
        {
            innerResult().code(RESTORE_FOOTPRINT_RESOURCE_LIMIT_EXCEEDED);
            return false;
        }

        rustEntryRentChanges.emplace_back();
        auto& rustChange = rustEntryRentChanges.back();
        rustChange.is_persistent = true;
        // Treat the entry as if it hasn't existed before restoration
        // for the rent fee purposes.
        rustChange.old_size_bytes = 0;
        rustChange.old_expiration_ledger = 0;
        rustChange.new_size_bytes = keySize + entrySize;
        rustChange.new_expiration_ledger = restoredExpirationLedger;
        setExpirationLedger(restoredEntry, restoredExpirationLedger);
        ltx.restore(restoredEntry);
    }
    int64_t rentFee = rust_bridge::compute_rent_fee(
        app.getConfig().CURRENT_LEDGER_PROTOCOL_VERSION,
        ltx.loadHeader().current().ledgerVersion, rustEntryRentChanges,
        app.getLedgerManager()
            .getSorobanNetworkConfig(ltx)
            .rustBridgeRentFeeConfiguration(),
        ledgerSeq);
    mParentTx.consumeRefundableSorobanResources(metrics.mLedgerReadByte * 2,
                                                rentFee);
    innerResult().code(RESTORE_FOOTPRINT_SUCCESS);
    return true;
}

bool
RestoreFootprintOpFrame::doCheckValid(SorobanNetworkConfig const& config,
                                      uint32_t ledgerVersion)
{
    auto const& footprint = mParentTx.sorobanResources().footprint;
    if (!footprint.readOnly.empty())
    {
        innerResult().code(RESTORE_FOOTPRINT_MALFORMED);
        return false;
    }

    for (auto const& lk : footprint.readWrite)
    {
        if (!isSorobanDataEntry(lk) || isTemporaryEntry(lk))
        {
            innerResult().code(RESTORE_FOOTPRINT_MALFORMED);
            return false;
        }
    }

    return true;
}

bool
RestoreFootprintOpFrame::doCheckValid(uint32_t ledgerVersion)
{
    throw std::runtime_error(
        "RestoreFootprintOpFrame::doCheckValid needs Config");
}

void
RestoreFootprintOpFrame::insertLedgerKeysToPrefetch(
    UnorderedSet<LedgerKey>& keys) const
{
}

bool
RestoreFootprintOpFrame::isSoroban() const
{
    return true;
}
}

#endif // ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
