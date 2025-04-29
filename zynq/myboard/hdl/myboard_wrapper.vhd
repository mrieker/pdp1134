--Copyright 1986-2018 Xilinx, Inc. All Rights Reserved.
----------------------------------------------------------------------------------
--Tool Version: Vivado v.2018.3 (lin64) Build 2405991 Thu Dec  6 23:36:41 MST 2018
--Host        : homepc2 running 64-bit Ubuntu 16.04.7 LTS
--Command     : generate_target myboard_wrapper.bd
--Design      : myboard_wrapper
--Purpose     : IP block netlist
----------------------------------------------------------------------------------
library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
library UNISIM;
use UNISIM.VCOMPONENTS.ALL;
entity myboard_wrapper is
  port (
    DDR_addr : inout STD_LOGIC_VECTOR ( 14 downto 0 );
    DDR_ba : inout STD_LOGIC_VECTOR ( 2 downto 0 );
    DDR_cas_n : inout STD_LOGIC;
    DDR_ck_n : inout STD_LOGIC;
    DDR_ck_p : inout STD_LOGIC;
    DDR_cke : inout STD_LOGIC;
    DDR_cs_n : inout STD_LOGIC;
    DDR_dm : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dq : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    DDR_dqs_n : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dqs_p : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_odt : inout STD_LOGIC;
    DDR_ras_n : inout STD_LOGIC;
    DDR_reset_n : inout STD_LOGIC;
    DDR_we_n : inout STD_LOGIC;
    FIXED_IO_ddr_vrn : inout STD_LOGIC;
    FIXED_IO_ddr_vrp : inout STD_LOGIC;
    FIXED_IO_mio : inout STD_LOGIC_VECTOR ( 53 downto 0 );
    FIXED_IO_ps_clk : inout STD_LOGIC;
    FIXED_IO_ps_porb : inout STD_LOGIC;
    FIXED_IO_ps_srstb : inout STD_LOGIC;
    LEDoutB_0 : out STD_LOGIC;
    LEDoutG_0 : out STD_LOGIC;
    LEDoutR_0 : out STD_LOGIC;
    a_out_h_0 : out STD_LOGIC_VECTOR ( 17 downto 0 );
    ac_lo_in_h_0 : in STD_LOGIC;
    ac_lo_out_h_0 : out STD_LOGIC;
    bbsy_in_h_0 : in STD_LOGIC;
    bbsy_out_h_0 : out STD_LOGIC;
    bg_in_l_0 : in STD_LOGIC_VECTOR ( 7 downto 4 );
    bg_out_l_0 : out STD_LOGIC_VECTOR ( 7 downto 4 );
    br_out_h_0 : out STD_LOGIC_VECTOR ( 7 downto 4 );
    c_out_h_0 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    d_out_h_0 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    dc_lo_in_h_0 : in STD_LOGIC;
    dc_lo_out_h_0 : out STD_LOGIC;
    hltgr_in_l_0 : in STD_LOGIC;
    hltrq_out_h_0 : out STD_LOGIC;
    init_in_h_0 : in STD_LOGIC;
    init_out_h_0 : out STD_LOGIC;
    intr_in_h_0 : in STD_LOGIC;
    intr_out_h_0 : out STD_LOGIC;
    msyn_in_h_0 : in STD_LOGIC;
    msyn_out_h_0 : out STD_LOGIC;
    muxa_0 : in STD_LOGIC;
    muxb_0 : in STD_LOGIC;
    muxc_0 : in STD_LOGIC;
    muxd_0 : in STD_LOGIC;
    muxe_0 : in STD_LOGIC;
    muxf_0 : in STD_LOGIC;
    muxh_0 : in STD_LOGIC;
    muxj_0 : in STD_LOGIC;
    muxk_0 : in STD_LOGIC;
    muxl_0 : in STD_LOGIC;
    muxm_0 : in STD_LOGIC;
    muxn_0 : in STD_LOGIC;
    muxp_0 : in STD_LOGIC;
    muxr_0 : in STD_LOGIC;
    muxs_0 : in STD_LOGIC;
    npg_in_l_0 : in STD_LOGIC;
    npg_out_l_0 : out STD_LOGIC;
    npr_out_h_0 : out STD_LOGIC;
    pa_out_h_0 : out STD_LOGIC;
    pb_out_h_0 : out STD_LOGIC;
    rsel1_h_0 : out STD_LOGIC;
    rsel2_h_0 : out STD_LOGIC;
    rsel3_h_0 : out STD_LOGIC;
    sack_in_h_0 : in STD_LOGIC;
    sack_out_h_0 : out STD_LOGIC;
    ssyn_in_h_0 : in STD_LOGIC;
    ssyn_out_h_0 : out STD_LOGIC
  );
end myboard_wrapper;

architecture STRUCTURE of myboard_wrapper is
  component myboard is
  port (
    DDR_addr : inout STD_LOGIC_VECTOR ( 14 downto 0 );
    DDR_ba : inout STD_LOGIC_VECTOR ( 2 downto 0 );
    DDR_cas_n : inout STD_LOGIC;
    DDR_ck_n : inout STD_LOGIC;
    DDR_ck_p : inout STD_LOGIC;
    DDR_cke : inout STD_LOGIC;
    DDR_cs_n : inout STD_LOGIC;
    DDR_dm : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dq : inout STD_LOGIC_VECTOR ( 31 downto 0 );
    DDR_dqs_n : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_dqs_p : inout STD_LOGIC_VECTOR ( 3 downto 0 );
    DDR_odt : inout STD_LOGIC;
    DDR_ras_n : inout STD_LOGIC;
    DDR_reset_n : inout STD_LOGIC;
    DDR_we_n : inout STD_LOGIC;
    FIXED_IO_ddr_vrn : inout STD_LOGIC;
    FIXED_IO_ddr_vrp : inout STD_LOGIC;
    FIXED_IO_mio : inout STD_LOGIC_VECTOR ( 53 downto 0 );
    FIXED_IO_ps_clk : inout STD_LOGIC;
    FIXED_IO_ps_porb : inout STD_LOGIC;
    FIXED_IO_ps_srstb : inout STD_LOGIC;
    LEDoutB_0 : out STD_LOGIC;
    LEDoutG_0 : out STD_LOGIC;
    LEDoutR_0 : out STD_LOGIC;
    a_out_h_0 : out STD_LOGIC_VECTOR ( 17 downto 0 );
    ac_lo_in_h_0 : in STD_LOGIC;
    ac_lo_out_h_0 : out STD_LOGIC;
    bbsy_in_h_0 : in STD_LOGIC;
    bbsy_out_h_0 : out STD_LOGIC;
    bg_in_l_0 : in STD_LOGIC_VECTOR ( 7 downto 4 );
    bg_out_l_0 : out STD_LOGIC_VECTOR ( 7 downto 4 );
    br_out_h_0 : out STD_LOGIC_VECTOR ( 7 downto 4 );
    c_out_h_0 : out STD_LOGIC_VECTOR ( 1 downto 0 );
    d_out_h_0 : out STD_LOGIC_VECTOR ( 15 downto 0 );
    dc_lo_in_h_0 : in STD_LOGIC;
    dc_lo_out_h_0 : out STD_LOGIC;
    hltgr_in_l_0 : in STD_LOGIC;
    hltrq_out_h_0 : out STD_LOGIC;
    init_in_h_0 : in STD_LOGIC;
    init_out_h_0 : out STD_LOGIC;
    intr_in_h_0 : in STD_LOGIC;
    intr_out_h_0 : out STD_LOGIC;
    msyn_in_h_0 : in STD_LOGIC;
    msyn_out_h_0 : out STD_LOGIC;
    muxa_0 : in STD_LOGIC;
    muxb_0 : in STD_LOGIC;
    muxc_0 : in STD_LOGIC;
    muxd_0 : in STD_LOGIC;
    muxe_0 : in STD_LOGIC;
    muxf_0 : in STD_LOGIC;
    muxh_0 : in STD_LOGIC;
    muxj_0 : in STD_LOGIC;
    muxk_0 : in STD_LOGIC;
    muxl_0 : in STD_LOGIC;
    muxm_0 : in STD_LOGIC;
    muxn_0 : in STD_LOGIC;
    muxp_0 : in STD_LOGIC;
    muxr_0 : in STD_LOGIC;
    muxs_0 : in STD_LOGIC;
    npg_in_l_0 : in STD_LOGIC;
    npg_out_l_0 : out STD_LOGIC;
    npr_out_h_0 : out STD_LOGIC;
    pa_out_h_0 : out STD_LOGIC;
    pb_out_h_0 : out STD_LOGIC;
    rsel1_h_0 : out STD_LOGIC;
    rsel2_h_0 : out STD_LOGIC;
    rsel3_h_0 : out STD_LOGIC;
    sack_in_h_0 : in STD_LOGIC;
    sack_out_h_0 : out STD_LOGIC;
    ssyn_in_h_0 : in STD_LOGIC;
    ssyn_out_h_0 : out STD_LOGIC
  );
  end component myboard;
begin
myboard_i: component myboard
  port map (
    DDR_addr(14 downto 0) => DDR_addr(14 downto 0),
    DDR_ba(2 downto 0) => DDR_ba(2 downto 0),
    DDR_cas_n => DDR_cas_n,
    DDR_ck_n => DDR_ck_n,
    DDR_ck_p => DDR_ck_p,
    DDR_cke => DDR_cke,
    DDR_cs_n => DDR_cs_n,
    DDR_dm(3 downto 0) => DDR_dm(3 downto 0),
    DDR_dq(31 downto 0) => DDR_dq(31 downto 0),
    DDR_dqs_n(3 downto 0) => DDR_dqs_n(3 downto 0),
    DDR_dqs_p(3 downto 0) => DDR_dqs_p(3 downto 0),
    DDR_odt => DDR_odt,
    DDR_ras_n => DDR_ras_n,
    DDR_reset_n => DDR_reset_n,
    DDR_we_n => DDR_we_n,
    FIXED_IO_ddr_vrn => FIXED_IO_ddr_vrn,
    FIXED_IO_ddr_vrp => FIXED_IO_ddr_vrp,
    FIXED_IO_mio(53 downto 0) => FIXED_IO_mio(53 downto 0),
    FIXED_IO_ps_clk => FIXED_IO_ps_clk,
    FIXED_IO_ps_porb => FIXED_IO_ps_porb,
    FIXED_IO_ps_srstb => FIXED_IO_ps_srstb,
    LEDoutB_0 => LEDoutB_0,
    LEDoutG_0 => LEDoutG_0,
    LEDoutR_0 => LEDoutR_0,
    a_out_h_0(17 downto 0) => a_out_h_0(17 downto 0),
    ac_lo_in_h_0 => ac_lo_in_h_0,
    ac_lo_out_h_0 => ac_lo_out_h_0,
    bbsy_in_h_0 => bbsy_in_h_0,
    bbsy_out_h_0 => bbsy_out_h_0,
    bg_in_l_0(7 downto 4) => bg_in_l_0(7 downto 4),
    bg_out_l_0(7 downto 4) => bg_out_l_0(7 downto 4),
    br_out_h_0(7 downto 4) => br_out_h_0(7 downto 4),
    c_out_h_0(1 downto 0) => c_out_h_0(1 downto 0),
    d_out_h_0(15 downto 0) => d_out_h_0(15 downto 0),
    dc_lo_in_h_0 => dc_lo_in_h_0,
    dc_lo_out_h_0 => dc_lo_out_h_0,
    hltgr_in_l_0 => hltgr_in_l_0,
    hltrq_out_h_0 => hltrq_out_h_0,
    init_in_h_0 => init_in_h_0,
    init_out_h_0 => init_out_h_0,
    intr_in_h_0 => intr_in_h_0,
    intr_out_h_0 => intr_out_h_0,
    msyn_in_h_0 => msyn_in_h_0,
    msyn_out_h_0 => msyn_out_h_0,
    muxa_0 => muxa_0,
    muxb_0 => muxb_0,
    muxc_0 => muxc_0,
    muxd_0 => muxd_0,
    muxe_0 => muxe_0,
    muxf_0 => muxf_0,
    muxh_0 => muxh_0,
    muxj_0 => muxj_0,
    muxk_0 => muxk_0,
    muxl_0 => muxl_0,
    muxm_0 => muxm_0,
    muxn_0 => muxn_0,
    muxp_0 => muxp_0,
    muxr_0 => muxr_0,
    muxs_0 => muxs_0,
    npg_in_l_0 => npg_in_l_0,
    npg_out_l_0 => npg_out_l_0,
    npr_out_h_0 => npr_out_h_0,
    pa_out_h_0 => pa_out_h_0,
    pb_out_h_0 => pb_out_h_0,
    rsel1_h_0 => rsel1_h_0,
    rsel2_h_0 => rsel2_h_0,
    rsel3_h_0 => rsel3_h_0,
    sack_in_h_0 => sack_in_h_0,
    sack_out_h_0 => sack_out_h_0,
    ssyn_in_h_0 => ssyn_in_h_0,
    ssyn_out_h_0 => ssyn_out_h_0
  );
end STRUCTURE;
