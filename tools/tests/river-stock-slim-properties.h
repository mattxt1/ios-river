/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* Extracted verbatim stock property functions/constants, with minimal host
 * type declarations. LineageOS/android_kernel_motorola_sdm632 lineage-18.1
 * drivers/slimbus/slimbus.c blob1140b33cf3144fe761ea8865ac2c2c38e53b28bc
 * include/linux/slimbus/slimbus.h blob4b5dc5452b201d6c05e19c68ef998a7ed53d8c99
 */
typedef uint32_t u32;
typedef uint8_t u8;
#define dev_dbg(...) ((void)0)
#define SLIM_CL_PER_SUPERFRAME		6144
#define SLIM_CL_PER_SUPERFRAME_DIV8	(SLIM_CL_PER_SUPERFRAME >> 3)
#define SLIM_CL_PER_SL			4
enum slim_ch_proto {
	SLIM_HARD_ISO,
	SLIM_AUTO_ISO,
	SLIM_PUSH,
	SLIM_PULL,
	SLIM_ASYNC_SMPLX,
	SLIM_ASYNC_HALF_DUP,
	SLIM_EXT_SMPLX,
	SLIM_EXT_HALF_DUP,
};
enum slim_ch_rate {
	SLIM_RATE_1HZ,
	SLIM_RATE_4000HZ,
	SLIM_RATE_11025HZ,
};
enum slim_ch_coeff {
	SLIM_COEFF_1,
	SLIM_COEFF_3,
};
struct slim_ch {enum slim_ch_proto prot;enum slim_ch_rate baser;u32 ratem,sampleszbits;};
struct slim_ich {struct slim_ch prop;u32 prrate,rootexp,seglen;enum slim_ch_coeff coeff;};
struct slim_framer {u32 rootfreq,superfreq;};
struct slim_controller {struct slim_framer *a_framer;struct slim_ich *chans;};
struct slim_device {struct slim_controller *ctrl;};
static u32 slim_calc_prrate(struct slim_controller *ctrl, struct slim_ch *prop)
{
	u32 rate = 0, rate4k = 0, rate11k = 0;
	u32 exp = 0;
	u32 pr = 0;
	bool exact = true;
	bool done = false;
	enum slim_ch_rate ratefam;

	if (prop->prot >= SLIM_ASYNC_SMPLX)
		return 0;
	if (prop->baser == SLIM_RATE_1HZ) {
		rate = prop->ratem / 4000;
		rate4k = rate;
		if (rate * 4000 == prop->ratem)
			ratefam = SLIM_RATE_4000HZ;
		else {
			rate = prop->ratem / 11025;
			rate11k = rate;
			if (rate * 11025 == prop->ratem)
				ratefam = SLIM_RATE_11025HZ;
			else
				ratefam = SLIM_RATE_1HZ;
		}
	} else {
		ratefam = prop->baser;
		rate = prop->ratem;
	}
	if (ratefam == SLIM_RATE_1HZ) {
		exact = false;
		if ((rate4k + 1) * 4000 < (rate11k + 1) * 11025) {
			rate = rate4k + 1;
			ratefam = SLIM_RATE_4000HZ;
		} else {
			rate = rate11k + 1;
			ratefam = SLIM_RATE_11025HZ;
		}
	}
	/* covert rate to coeff-exp */
	while (!done) {
		while ((rate & 0x1) != 0x1) {
			rate >>= 1;
			exp++;
		}
		if (rate > 3) {
			/* roundup if not exact */
			rate++;
			exact = false;
		} else
			done = true;
	}
	if (ratefam == SLIM_RATE_4000HZ) {
		if (rate == 1)
			pr = 0x10;
		else {
			pr = 0;
			exp++;
		}
	} else {
		pr = 8;
		exp++;
	}
	if (exp <= 7) {
		pr |= exp;
		if (exact)
			pr |= 0x80;
	} else
		pr = 0;
	return pr;
}

static int slim_nextdefine_ch(struct slim_device *sb, u8 chan)
{
	struct slim_controller *ctrl = sb->ctrl;
	u32 chrate = 0;
	u32 exp = 0;
	u32 coeff = 0;
	bool exact = true;
	bool done = false;
	int ret = 0;
	struct slim_ich *slc = &ctrl->chans[chan];
	struct slim_ch *prop = &slc->prop;

	slc->prrate = slim_calc_prrate(ctrl, prop);
	dev_dbg(&ctrl->dev, "ch:%d, chan PR rate:%x\n", chan, slc->prrate);
	if (prop->baser == SLIM_RATE_4000HZ)
		chrate = 4000 * prop->ratem;
	else if (prop->baser == SLIM_RATE_11025HZ)
		chrate = 11025 * prop->ratem;
	else
		chrate = prop->ratem;
	/* max allowed sample freq = 768 seg/frame */
	if (chrate > 3600000)
		return -EDQUOT;
	if (prop->baser == SLIM_RATE_4000HZ &&
			ctrl->a_framer->superfreq == 4000)
		coeff = prop->ratem;
	else if (prop->baser == SLIM_RATE_11025HZ &&
			ctrl->a_framer->superfreq == 3675)
		coeff = 3 * prop->ratem;
	else {
		u32 tempr = 0;

		tempr = chrate * SLIM_CL_PER_SUPERFRAME_DIV8;
		coeff = tempr / ctrl->a_framer->rootfreq;
		if (coeff * ctrl->a_framer->rootfreq != tempr) {
			coeff++;
			exact = false;
		}
	}

	/* convert coeff to coeff-exponent */
	exp = 0;
	while (!done) {
		while ((coeff & 0x1) != 0x1) {
			coeff >>= 1;
			exp++;
		}
		if (coeff > 3) {
			coeff++;
			exact = false;
		} else
			done = true;
	}
	if (prop->prot == SLIM_HARD_ISO && !exact)
		return -EPROTONOSUPPORT;
	else if (prop->prot == SLIM_AUTO_ISO) {
		if (exact)
			prop->prot = SLIM_HARD_ISO;
		else
			prop->prot = SLIM_PUSH;
	}
	slc->rootexp = exp;
	slc->seglen = prop->sampleszbits/SLIM_CL_PER_SL;
	if (prop->prot != SLIM_HARD_ISO)
		slc->seglen++;
	if (prop->prot >= SLIM_EXT_SMPLX)
		slc->seglen++;
	/* convert coeff to enum */
	if (coeff == 1) {
		if (exp > 9)
			ret = -EIO;
		coeff = SLIM_COEFF_1;
	} else {
		if (exp > 8)
			ret = -EIO;
		coeff = SLIM_COEFF_3;
	}
	slc->coeff = coeff;

	return ret;
}

static struct slim_ich stock_channel_properties(unsigned rate)
{
 struct slim_ich channel={.prop={.prot=SLIM_AUTO_ISO,.baser=SLIM_RATE_4000HZ,.ratem=rate/4000,.sampleszbits=16}};
 struct slim_framer framer={.rootfreq=24576000>>3};framer.superfreq=framer.rootfreq/SLIM_CL_PER_SUPERFRAME_DIV8;
 struct slim_controller controller={.a_framer=&framer,.chans=&channel};
 struct slim_device device={.ctrl=&controller};assert(!slim_nextdefine_ch(&device,0));return channel;
}
