# Context Dossier: Third-Party USB Power Delivery 3.0 / PPS Fast Charging, USB OTG Warm Reset Fix, and DTS Force Warm Reboot

## Subsystem Architecture
- **Components**:
  - `drivers/power/supply/ti/pd_policy_manager.c`
  - `drivers/power/supply/qcom/qpnp-smb5.c`
  - `drivers/power/supply/qcom_cas/qpnp-smb5.c`
  - `arch/arm64/boot/dts/vendor/qcom/kona.dtsi`
- **Architectural Scope**:
  1. **Third-Party USB-PD 3.0 & PPS Fast Charging (up to 33W)**:
     - Bypasses proprietary vendor authentication checks in `pd_get_bms_digest_verified`, `pd_get_bms_chip_ok`, `pd_get_fastcharge_mode_enabled`, and `pd_get_pps_charger_verified`.
     - Enables negotiation of USB-PD 3.0 and PPS fast charging profiles on standard non-Xiaomi charging bricks (Anker, Ugreen, Apple, Samsung, GaN powerbanks) while strictly preserving hardware safety cutoffs, thermal throttling, and voltage thresholds managed in `usbpd_pm_sm`.
  2. **USB OTG Warm Reset Fix**:
     - Explicitly resets and disables the VBUS regulator (`smblib_masked_write(chg, DCDC_CMD_OTG_REG, OTG_EN_BIT, 0)`) in `smb5_shutdown()`.
     - Eliminates USB host-mode controller state latching and freezes when the device reboots with USB drives or OTG peripherals connected.
  3. **Qualcomm Kona DTS Force Warm Reboot**:
     - Configures `qcom,force-warm-reboot;` in `restart@c264000` (`qcom,pshold`).
     - Preserves RAM state across reboots for pstore/ramoops panic logs and reduces warm reboot latency.
