#pragma once

#include "core/device/device.h"
#include <functional>
#include <cstdint>

// ACPI PM register emulation with PM Timer support.
// Provides PM1a Event Block, PM1a Control Block, and PM Timer.
class AcpiPm : public Device {
public:
    static constexpr uint16_t kBasePort   = 0x600;
    static constexpr uint16_t kRegCount   = 16;   // EVT(4) + CNT(2) + gap(2) + RESET(1) + gap(3) + TMR(4)
    static constexpr uint16_t kEvtPort    = 0x600; // PM1a_EVT_BLK
    static constexpr uint16_t kCntPort    = 0x604; // PM1a_CNT_BLK
    static constexpr uint16_t kTmrPort    = 0x608; // PM_TMR_BLK (moved RESET to 0x60F)
    static constexpr uint16_t kResetPort  = 0x60F; // ACPI RESET_REG
    static constexpr uint8_t  kEvtLen     = 4;
    static constexpr uint8_t  kCntLen     = 2;
    static constexpr uint8_t  kTmrLen     = 4;
    static constexpr uint8_t  kSlpTypS5   = 5;    // Must match DSDT \_S5 package
    static constexpr uint8_t  kResetValue = 0x01; // Value to trigger reset

    // PM Timer runs at 3.579545 MHz (ACPI spec)
    static constexpr uint32_t kPmTimerFreq = 3579545;

    AcpiPm();

    void SetShutdownCallback(std::function<void()> cb) { shutdown_cb_ = std::move(cb); }
    void SetResetCallback(std::function<void()> cb) { reset_cb_ = std::move(cb); }
    void SetSciCallback(std::function<void()> cb) { sci_cb_ = std::move(cb); }

    void TriggerPowerButton();

    void PioRead(uint16_t offset, uint8_t size, uint32_t* value) override;
    void PioWrite(uint16_t offset, uint8_t size, uint32_t value) override;

private:
    void RaiseSci();
    uint32_t ReadPmTimer() const;

    uint16_t pm1_sts_ = 0;
    uint16_t pm1_en_  = 0;
    uint16_t pm1_cnt_ = 1; // SCI_EN (bit 0) always set
    uint64_t tsc_freq_ = 0;
    uint64_t tsc_base_ = 0;
    std::function<void()> shutdown_cb_;
    std::function<void()> reset_cb_;
    std::function<void()> sci_cb_;
};
