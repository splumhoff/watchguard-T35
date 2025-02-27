/* $Date$ $RCSfile: mv88e1xxx.h,v $ $Revision$ */
#ifndef CHELSIO_MV8E1XXX_H
#define CHELSIO_MV8E1XXX_H

#ifndef BMCR_SPEED1000
# define BMCR_SPEED1000 0x40
#endif

#ifndef ADVERTISE_PAUSE
# define ADVERTISE_PAUSE 0x400
#endif
#ifndef ADVERTISE_PAUSE_ASYM
# define ADVERTISE_PAUSE_ASYM 0x800
#endif

/* Gigabit MII registers */
#define MII_GBCR 9       /* 1000Base-T control register */
#define MII_GBSR 10      /* 1000Base-T status register */

/* 1000Base-T control register fields */
#define GBCR_ADV_1000HALF         0x100
#define GBCR_ADV_1000FULL         0x200
#define GBCR_PREFER_MASTER        0x400
#define GBCR_MANUAL_AS_MASTER     0x800
#define GBCR_MANUAL_CONFIG_ENABLE 0x1000

/* 1000Base-T status register fields */
#define GBSR_LP_1000HALF  0x400
#define GBSR_LP_1000FULL  0x800
#define GBSR_REMOTE_OK    0x1000
#define GBSR_LOCAL_OK     0x2000
#define GBSR_LOCAL_MASTER 0x4000
#define GBSR_MASTER_FAULT 0x8000

/* Marvell PHY interrupt status bits. */
#define MV88E1XXX_INTR_JABBER          0x0001
#define MV88E1XXX_INTR_POLARITY_CHNG   0x0002
#define MV88E1XXX_INTR_ENG_DETECT_CHNG 0x0010
#define MV88E1XXX_INTR_DOWNSHIFT       0x0020
#define MV88E1XXX_INTR_MDI_XOVER_CHNG  0x0040
#define MV88E1XXX_INTR_FIFO_OVER_UNDER 0x0080
#define MV88E1XXX_INTR_FALSE_CARRIER   0x0100
#define MV88E1XXX_INTR_SYMBOL_ERROR    0x0200
#define MV88E1XXX_INTR_LINK_CHNG       0x0400
#define MV88E1XXX_INTR_AUTONEG_DONE    0x0800
#define MV88E1XXX_INTR_PAGE_RECV       0x1000
#define MV88E1XXX_INTR_DUPLEX_CHNG     0x2000
#define MV88E1XXX_INTR_SPEED_CHNG      0x4000
#define MV88E1XXX_INTR_AUTONEG_ERR     0x8000

/* Marvell PHY specific registers. */
#define MV88E1XXX_SPECIFIC_CNTRL_REGISTER               16
#define MV88E1XXX_SPECIFIC_STATUS_REGISTER              17
#define MV88E1XXX_INTERRUPT_ENABLE_REGISTER             18
#define MV88E1XXX_INTERRUPT_STATUS_REGISTER             19
#define MV88E1XXX_EXT_PHY_SPECIFIC_CNTRL_REGISTER       20
#define MV88E1XXX_RECV_ERR_CNTR_REGISTER                21
#define MV88E1XXX_RES_REGISTER                          22
#define MV88E1XXX_GLOBAL_STATUS_REGISTER                23
#define MV88E1XXX_LED_CONTROL_REGISTER                  24
#define MV88E1XXX_MANUAL_LED_OVERRIDE_REGISTER          25
#define MV88E1XXX_EXT_PHY_SPECIFIC_CNTRL_2_REGISTER     26
#define MV88E1XXX_EXT_PHY_SPECIFIC_STATUS_REGISTER      27
#define MV88E1XXX_VIRTUAL_CABLE_TESTER_REGISTER         28
#define MV88E1XXX_EXTENDED_ADDR_REGISTER                29
#define MV88E1XXX_EXTENDED_REGISTER                     30

/* PHY specific control register fields */
#define S_PSCR_MDI_XOVER_MODE    5
#define M_PSCR_MDI_XOVER_MODE    0x3
#define V_PSCR_MDI_XOVER_MODE(x) ((x) << S_PSCR_MDI_XOVER_MODE)
#define G_PSCR_MDI_XOVER_MODE(x) (((x) >> S_PSCR_MDI_XOVER_MODE) & M_PSCR_MDI_XOVER_MODE)

/* Extended PHY specific control register fields */
#define S_DOWNSHIFT_ENABLE 8
#define V_DOWNSHIFT_ENABLE (1 << S_DOWNSHIFT_ENABLE)

#define S_DOWNSHIFT_CNT    9
#define M_DOWNSHIFT_CNT    0x7
#define V_DOWNSHIFT_CNT(x) ((x) << S_DOWNSHIFT_CNT)
#define G_DOWNSHIFT_CNT(x) (((x) >> S_DOWNSHIFT_CNT) & M_DOWNSHIFT_CNT)

/* PHY specific status register fields */
#define S_PSSR_JABBER 0
#define V_PSSR_JABBER (1 << S_PSSR_JABBER)

#define S_PSSR_POLARITY 1
#define V_PSSR_POLARITY (1 << S_PSSR_POLARITY)

#define S_PSSR_RX_PAUSE 2
#define V_PSSR_RX_PAUSE (1 << S_PSSR_RX_PAUSE)

#define S_PSSR_TX_PAUSE 3
#define V_PSSR_TX_PAUSE (1 << S_PSSR_TX_PAUSE)

#define S_PSSR_ENERGY_DETECT 4
#define V_PSSR_ENERGY_DETECT (1 << S_PSSR_ENERGY_DETECT)

#define S_PSSR_DOWNSHIFT_STATUS 5
#define V_PSSR_DOWNSHIFT_STATUS (1 << S_PSSR_DOWNSHIFT_STATUS)

#define S_PSSR_MDI 6
#define V_PSSR_MDI (1 << S_PSSR_MDI)

#define S_PSSR_CABLE_LEN    7
#define M_PSSR_CABLE_LEN    0x7
#define V_PSSR_CABLE_LEN(x) ((x) << S_PSSR_CABLE_LEN)
#define G_PSSR_CABLE_LEN(x) (((x) >> S_PSSR_CABLE_LEN) & M_PSSR_CABLE_LEN)

#define S_PSSR_LINK 10
#define V_PSSR_LINK (1 << S_PSSR_LINK)

#define S_PSSR_STATUS_RESOLVED 11
#define V_PSSR_STATUS_RESOLVED (1 << S_PSSR_STATUS_RESOLVED)

#define S_PSSR_PAGE_RECEIVED 12
#define V_PSSR_PAGE_RECEIVED (1 << S_PSSR_PAGE_RECEIVED)

#define S_PSSR_DUPLEX 13
#define V_PSSR_DUPLEX (1 << S_PSSR_DUPLEX)

#define S_PSSR_SPEED    14
#define M_PSSR_SPEED    0x3
#define V_PSSR_SPEED(x) ((x) << S_PSSR_SPEED)
#define G_PSSR_SPEED(x) (((x) >> S_PSSR_SPEED) & M_PSSR_SPEED)

#endif
