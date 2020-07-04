/* scarlett mixer GUI
 *
 * Copyright 2015-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef DEFAULT_DEVICE
#define DEFAULT_DEVICE "hw:2"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>

#define RTK_URI "http://gareus.org/oss/scarlettmixer#"
#define RTK_GUI "ui"

#define GD_WIDTH 41
#define GD_CX 20.5
#define GD_CY 15.5

/* device specifics, see also
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/sound/usb/mixer_scarlett.c#n635
 */

#define MAX_GAINS   10
#define MAX_BUSSES  20
#define MAX_HIZS    2
#define MAX_PADS    4
#define MAX_AIRS    2

typedef struct {
	char        name[64];
	unsigned    smi;  //< mixer matrix inputs
	unsigned    smo;  //< mixer matrix outputs
	unsigned    sin;  //< inputs (capture select)
	unsigned    sout; //< outputs assigns
	unsigned    smst; //< main outputs (stereo gain controls w/mute =?= sout / 2)
	unsigned    samo; //< aux outputs (mono gain controls w/o mute)

	unsigned    num_hiz;
	unsigned    num_pad;
	unsigned    num_air;
	bool	    pads_are_switches;
	unsigned    matrix_mix_offset;
	unsigned    matrix_mix_stride;
	bool	    matrix_mix_column_major;
	unsigned    matrix_in_offset;
	unsigned    matrix_in_stride;
	unsigned    input_offset;
	int         out_gain_map[MAX_GAINS];
	char        out_gain_labels[MAX_GAINS][16];
	int         out_bus_map[MAX_BUSSES];
	int         hiz_map[MAX_HIZS];
	int         pad_map[MAX_PADS];
	int         air_map[MAX_AIRS];
} Device;

static Device devices[] = {
	{
		.name = "Scarlett 18i6 USB",
		.smi = 18, .smo = 6,
		.sin = 18, .sout = 6,
		.smst = 3,
		.num_hiz = 2,
		.num_pad = 0,
		.matrix_mix_offset = 33, .matrix_mix_stride = 7,
		.matrix_in_offset = 32, .matrix_in_stride = 7,
		.input_offset = 14,
		.out_gain_map = { 1 /* Monitor */, 4 /* Headphone */, 7 /* SPDIF */, -1, -1 , -1, -1, -1, -1, -1 }, // PBS
		.out_gain_labels = { "Monitor", "Headphone", "SPDIF", "", "", "", "", "", "", "" },
		.out_bus_map = { 2, 3, 5, 6, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }, // Source, ENUM
		.hiz_map = { 12, 13 },
		.pad_map = { -1, -1, -1, -1 },
	},
	{
		.name = "Scarlett 18i8 USB",
		.smi = 18, .smo = 8,
		.sin = 18, .sout = 8,
		.smst = 4,
		.num_hiz = 2,
		.num_pad = 4,
		.matrix_mix_offset = 40, .matrix_mix_stride = 9, // < Matrix 01 Mix A
		.matrix_in_offset = 39, .matrix_in_stride = 9,   // Matrix 01 Input, ENUM
		.input_offset = 21,   // < Input Source 01, ENUM
		.out_gain_map = { 1 /* Monitor */, 4 /* Headphone 1 */, 7 /* Headphone 2 */, 10 /* SPDIF */, -1, -1 , -1, -1, -1, -1 },
		.out_gain_labels = { "Monitor", "Headphone 1", "Headphone 2", "SPDIF", "", "", "", "", "", "" },
		.out_bus_map = { 2, 3, 5, 6, 8, 9, 11, 12, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
		.hiz_map = { 15, 17 }, // < Input 1 Impedance, ENUM,  Input 2 Impedance, ENUM
		.pad_map = { 16, 18, 19, 20 },
	},
	{
		.name = "Scarlett 6i6 USB",
		.smi = 6, .smo = 6,
		.sin = 6, .sout = 6,
		.smst = 3,
		.num_hiz = 2,
		.num_pad = 4, // XXX does the device have pad? bug in kernel-driver?
		.matrix_mix_offset = 26, .matrix_mix_stride = 9, // XXX stride should be 7, bug in kernel-driver ?!
		.matrix_in_offset = 25, .matrix_in_stride = 9,   // XXX stride should be 7, bug in kernel-driver ?!
		.out_gain_map = { 1 /* Monitor */, 4 /* Headphone */, 7 /* SPDIF */, -1, -1, -1 , -1, -1, -1, -1 },
		.out_gain_labels = { "Monitor", "Headphone", "SPDIF", "", "", "", "", "", "", "" },
		.out_bus_map = { 2, 3, 5, 6, 8, 9, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
		.input_offset = 18,
		.hiz_map = { 12, 14 },
		.pad_map = { 13, 15, 16, 17 },
	},
	{
		.name = "Scarlett 18i20 USB",
		.smi = 18, .smo = 8,
		.sin = 18, .sout = 20,
		.smst = 10,
		.num_hiz = 0,
		.num_pad = 0,
		.matrix_mix_offset = 50, .matrix_mix_stride = 9,
		.matrix_in_offset = 49, .matrix_in_stride = 9,
		.input_offset = 31,
		.out_gain_map = { 1, 7, 10, 13, 16, 19, 22, 25, 28, 2  },
		.out_gain_labels = { "Monitor", "Line 3/4", "Line 5/6", "Line 7/8", "Line 9/10" , "SPDIF", "ADAT 1/2", "ADAT 3/4", "ADAT 5/6", "ADAT 7/8" },
		.out_bus_map = { 5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20, 21, 23, 24, 26, 27, 29, 30, 3, 4 },
		.hiz_map = { -1, -1 },
		.pad_map = { -1, -1, -1, -1 },
	},
	{
		.name = "Scarlett 8i6 USB",
		.smi = 8, .smo = 8,
		.sin = 10, .sout = 6,
		.smst = 0,
		.samo = 4,
		.num_hiz = 2,
		.num_pad = 2, 
		.num_air = 2, 
		.pads_are_switches = true,
		.matrix_mix_offset = 20, .matrix_mix_stride = 8,
		.matrix_in_offset = 84, .matrix_in_stride = 1,
		.matrix_mix_column_major = true,
		.out_gain_map = { 10 /* Headphone 1 */, 11, 12 /* Headphone 2 */, 13, -1, -1 , -1, -1, -1, -1 },
		.out_gain_labels = { "Headphone 1L", "Headphone 1R", "Headphone 2L", "Headphone 2R", "SPDIF/L", "SPDIF/R", "", "", "", "" },
		.out_bus_map = { 92, 93, 94, 95, 97, 98, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
		.input_offset = 0,
		.hiz_map = { 15, 18 },
		.pad_map = { 16, 19, -1, -1 },
		.air_map = { 14, 17 },
	},
};

#define NUM_DEVICES     (sizeof (devices) / sizeof (devices[0]))

typedef struct {
	snd_mixer_elem_t* elem;
	char* name;
} Mctrl;

typedef struct {
	RobWidget*      rw;
	RobWidget*      matrix;
	RobWidget*      output;
	RobTkSelect**   mtx_sel;
	RobTkDial**     mtx_gain;
	RobTkLbl**      mtx_lbl;

	RobTkSep*       sep_h;
	RobTkSep*       sep_v;
	RobTkSep*       spc_v[2];

	RobTkLbl**      src_lbl;
	RobTkSelect**   src_sel;

	RobTkSelect**   out_sel;
	RobTkLbl*       out_mst;
	RobTkLbl**      out_lbl;
	RobTkDial**     out_gain;
	RobTkLbl**      aux_lbl;
	RobTkDial**     aux_gain;
	RobTkLbl**      sel_lbl;

	RobTkDial*      mst_gain;
	RobTkCBtn**     btn_hiz;
	RobTkCBtn**     btn_pad;
	RobTkCBtn**     btn_air;
	RobTkPBtn*      btn_reset;

	RobTkLbl*       heading[3];

	PangoFontDescription* font;
	cairo_surface_t*      mtx_sf[6];

	Device*      device;
	Mctrl*       ctrl;
	unsigned int ctrl_cnt;
	snd_mixer_t* mixer;

	int nfds;
	struct pollfd* pollfds;
	bool disable_signals;
} RobTkApp;


/* *****************************************************************************
 * Mapping for the 18i6 and 18i8
 *
 * NOTE: these are numerically hardcoded. see `amixer -D hw:2 control`
 * and #if'd "Print Controls" debug dump below
 */

/* mixer-matrix ; colums(src) x rows (dest) */
static Mctrl* matrix_ctrl_cr (RobTkApp* ui, unsigned int c, unsigned int r)
{
	unsigned int ctrl_id;
	/* Matrix 01 Mix A
	 *  ..
	 * Matrix 18 Mix F
	 */
	if (r >= ui->device->smi || c >= ui->device->smo) {
		return NULL;
	}
	if (ui->device->matrix_mix_column_major)
		ctrl_id = ui->device->matrix_mix_offset + c * ui->device->matrix_mix_stride + r;
	else
		ctrl_id = ui->device->matrix_mix_offset + r * ui->device->matrix_mix_stride + c;
	return &ui->ctrl[ctrl_id];
}

/* wrapper to the above, linear lookup */
static Mctrl* matrix_ctrl_n (RobTkApp* ui, unsigned int n)
{
	unsigned c = n % ui->device->smo;
	unsigned r = n / ui->device->smo;
	return matrix_ctrl_cr (ui, c, r);
}

/* matrix input selector (per row)*/
static Mctrl* matrix_sel (RobTkApp* ui, unsigned int r)
{
	if (r >= ui->device->smi) {
		return NULL;
	}
	/* Matrix 01 Input, ENUM
	 *  ..
	 * Matrix 18 Input, ENUM
	 */
	unsigned int ctrl_id = ui->device->matrix_in_offset + r * ui->device->matrix_in_stride;
	return &ui->ctrl[ctrl_id];
}

/* Input/Capture selector */
static Mctrl* src_sel (RobTkApp* ui, unsigned int r)
{
	if (r >= ui->device->sin) {
		return NULL;
	}
	/* Input Source 01, ENUM
	 *  ..
	 * Input Source 18, ENUM
	 */
	unsigned int ctrl_id = ui->device->input_offset + r;
	return &ui->ctrl[ctrl_id];
}

static int src_sel_default (unsigned int r, int max_values)
{
	/* 0 <= r < ui->device->sin;  return 0 .. max_values - 1 */
	return (r + 7) % max_values; // XXX hardcoded defaults. offset 7: "Analog 1"
}

/* Output Gains */
static Mctrl* out_gain (RobTkApp* ui, unsigned int c)
{
	assert (c < MAX_GAINS);
	return &ui->ctrl[ui->device->out_gain_map[c]];
}

static const char* out_gain_label (RobTkApp *ui, int n)
{
	return ui->device->out_gain_labels[n];
}

static Mctrl* aux_gain (RobTkApp* ui, unsigned int c)
{
	assert (c < MAX_GAINS);
	return &ui->ctrl[ui->device->out_gain_map[c + ui->device->smst]];
}

static const char* aux_gain_label (RobTkApp *ui, int n)
{
	return ui->device->out_gain_labels[n + ui->device->smst];
}

static const char* out_select_label (RobTkApp *ui, int n)
{
	return ui->device->out_gain_labels[n + ui->device->smst + ui->device->samo];
}

/* Output Bus assignment (matrix-out to master) */
static Mctrl* out_sel (RobTkApp* ui, unsigned int c)
{
	assert (c < MAX_BUSSES);
	return &ui->ctrl[ui->device->out_bus_map[c]];
}

static int out_sel_default (unsigned int c)
{
	/* 0 <= c < ui->device->sout; */
	return 25 + c; // XXX hardcoded defaults. offset 25: "Mix 1"
}

/* Hi-Z switches */
static Mctrl* hiz (RobTkApp* ui, unsigned int c)
{
	assert (c < ui->device->num_hiz);
	return &ui->ctrl[ui->device->hiz_map[c]];
}

/* Pad switches */
static Mctrl* pad (RobTkApp *ui, unsigned c)
{
	assert (c < ui->device->num_pad);
	return &ui->ctrl[ui->device->pad_map[c]];
}

/* Air switches */
static Mctrl* air (RobTkApp *ui, unsigned c)
{
	assert (c < ui->device->num_air);
	return &ui->ctrl[ui->device->air_map[c]];
}

/* master gain */
static Mctrl* mst_gain (RobTkApp* ui)
{
	return &ui->ctrl[0]; /* Master, PBS */
}


/* *****************************************************************************
 * *****************************************************************************
 *
 * CODE FROM HERE ON SHOULD BE GENERIC
 *
 * *****************************************************************************
 * ****************************************************************************/

static int verbose = 0;

#define OPT_PROBE (1<<0)
#define OPT_DETECT (1<<1)

static void dump_device_desc (Device const* const d)
{
	printf ("--- Device: %s\n", d->name);
	printf ("Matrix: in=%d, out=%d, off=%d, stride=%d\n",
			d->smi, d->smo, d->matrix_mix_offset, d->matrix_mix_stride);
	printf ("Matrix: input-select=%d, select-stride=%d\n",
			d->matrix_in_offset, d->matrix_in_stride);
	printf ("Inputs: ins=%d select-offset=%d\n",
			d->sin, d->input_offset);
	printf ("Masters: n_mst=%d n_out-select=%d\n",
			d->smst, d->sout);
	printf ("Switches: n_pad=%d, n_hiz=%d\n",
			d->num_pad, d->num_hiz);

#define DUMP_ARRAY(name, len, fmt)  \
  printf (#name " = {");            \
  for (int i = 0; i < len; ++i) {   \
    printf (fmt ", ", d->name[i]);  \
  }                                 \
  printf ("};\n");

	DUMP_ARRAY (hiz_map, MAX_HIZS, "%d");
	DUMP_ARRAY (pad_map, MAX_PADS, "%d");
	DUMP_ARRAY (out_gain_map, MAX_GAINS, "%d");
	DUMP_ARRAY (out_gain_labels, MAX_GAINS, "%s");
	DUMP_ARRAY (out_bus_map, MAX_BUSSES, "%d");
	printf ("---\n");
}

/* *****************************************************************************
 * Alsa Mixer Interface
 */

static int open_mixer (RobTkApp* ui, const char* card, int opts)
{
	int rv = 0;
	int err;
	snd_mixer_selem_id_t *sid;
	snd_mixer_elem_t *elem;
	snd_mixer_selem_id_alloca (&sid);

	snd_ctl_t *hctl;
	snd_ctl_card_info_t *card_info;
	snd_ctl_card_info_alloca (&card_info);

	if ((err = snd_ctl_open (&hctl, card, 0)) < 0) {
		fprintf (stderr, "Control device %s open error: %s\n", card, snd_strerror (err));
		return err;
	}

	if ((err = snd_ctl_card_info (hctl, card_info)) < 0) {
		fprintf (stderr, "Control device %s hw info error: %s\n", card, snd_strerror (err));
		return err;
	}

	const char* card_name = snd_ctl_card_info_get_name (card_info);
	snd_ctl_close (hctl);

	if (!card_name) {
		fprintf (stderr, "Device `%s' is unknown\n", card);
		return -1;
	}

	ui->device = NULL;

	for (unsigned i = 0; i < NUM_DEVICES; i++) {
		if (!strcmp (card_name, devices[i].name))
			ui->device = &devices[i];
	}

	if (ui->device == NULL) {
		fprintf (stderr, "Device `%s' is not supported\n", card);
		rv = -1;
		if ((opts & OPT_PROBE) == 0) {
			return -1;
		}
	}

	if ((err = snd_mixer_open (&ui->mixer, 0)) < 0) {
		fprintf (stderr, "Mixer %s open error: %s\n", card, snd_strerror (err));
		return err;
	}
	if ((err = snd_mixer_attach (ui->mixer, card)) < 0) {
		fprintf (stderr, "Mixer attach %s error: %s\n", card, snd_strerror (err));
		snd_mixer_close (ui->mixer);
		return err;
	}
	if ((err = snd_mixer_selem_register (ui->mixer, NULL, NULL)) < 0) {
		fprintf (stderr, "Mixer register error: %s\n", snd_strerror (err));
		snd_mixer_close (ui->mixer);
		return err;
	}
	err = snd_mixer_load (ui->mixer);
	if (err < 0) {
		fprintf (stderr, "Mixer %s load error: %s\n", card, snd_strerror (err));
		snd_mixer_close (ui->mixer);
		return err;
	}

	int cnt = 0;

	for (elem = snd_mixer_first_elem (ui->mixer); elem; elem = snd_mixer_elem_next (elem)) {
		if (!snd_mixer_selem_is_active (elem)) {
			continue;
		}
		++cnt;
	}

	ui->ctrl_cnt = cnt;

	if (cnt == 0) {
		fprintf (stderr, "Mixer %s: no controls found\n", card);
		return -1;
	}

	if (opts & OPT_PROBE) {
		fprintf (stderr, "Device `%s' has %d contols: \n", card_name, cnt);
	}

	ui->ctrl = (Mctrl*)calloc (cnt, sizeof (Mctrl));

	Device d;
	memset (&d, 0, sizeof (Device));
	strncpy (d.name, card_name, 63);
	for (int i = 0; i < MAX_GAINS; ++i) { d.out_gain_map[i] = -1; }
	for (int i = 0; i < MAX_BUSSES; ++i) { d.out_bus_map[i] = -1; }
	for (int i = 0; i < MAX_HIZS; ++i) { d.hiz_map[i] = -1; }
	for (int i = 0; i < MAX_PADS; ++i) { d.pad_map[i] = -1; }
	int obm = 0;

	int i = 0;
	for (elem = snd_mixer_first_elem (ui->mixer); elem; elem = snd_mixer_elem_next (elem)) {
		if (!snd_mixer_selem_is_active (elem)) {
			continue;
		}

		Mctrl* c = &ui->ctrl[i];
		c->elem = elem;
		c->name = strdup (snd_mixer_selem_get_name (elem));

		if (opts & OPT_DETECT) {
			if (snd_mixer_selem_is_enumerated (elem)) {
				if (strstr (c->name, " Impedance") || strstr (c->name, " Level")) {
					d.hiz_map[d.num_hiz++] = i;
				}
				if (strstr (c->name, " Pad")) {
					d.pad_map[d.num_pad++] = i;
				}
				if (strstr (c->name, "Input Source 01") || strstr (c->name, "PCM 01")) {
					assert (d.input_offset == 0);
					d.input_offset = i;
				}
				if (strstr (c->name, "Input Source") || strstr (c->name, "PCM ")) {
					++d.sin;
				}
				if (strstr (c->name, "Matrix 01 Input") || (strstr (c->name, "Mixer Input 01"))) {
					assert (d.matrix_in_offset == 0);
					d.matrix_in_offset = i;
				}
				if ((strstr (c->name, "Matrix ") || (strstr (c->name, "Mixer "))) && strstr (c->name, " Input")) {
					++d.smi;
				}
				if (strstr (c->name, "Master ") || strstr (c->name, " Output")) { // Source enum
					d.out_bus_map[obm++] = i;
				}
			} else if (snd_mixer_selem_has_playback_switch (elem)) {
				if (strstr (c->name, "Master ")) {
					char* t1 = strchr (c->name, '(');
					char* t2 = t1 ? strchr (t1, ')') : NULL;
					if (t2) {
						++t1;
						strncpy (d.out_gain_labels[d.smst], t1, t2 - t1);
						d.out_gain_labels[d.smst][t2 - t1] = '\0';
					}
					d.out_gain_map[d.smst++] = i;
					d.sout = d.smst * 2;
				} else if (strstr (c->name, " Output")) {
					char* t1 = strstr(c->name, " Output");
					char* t2 = t1 ? strchr (t1 + 1, ' ') : NULL;
					if (t2) {
						strncpy (d.out_gain_labels[d.smst], c->name, t1 - c->name);
						d.out_gain_labels[d.smst][t1 - c->name + 1] = '\0';
						strcat (d.out_gain_labels[d.smst], t2);
					}
					d.out_gain_map[d.smst++] = i;
					d.sout = d.smst * 2;
				}
			} else if (snd_mixer_selem_has_capture_switch (elem)) {
				if (strstr (c->name, " Pad")) {
					d.pad_map[d.num_pad++] = i;
				}
			} else {
				if (strstr (c->name, "Matrix 01 Mix A") || strstr (c->name, "Mix A Input 01")) {
					d.matrix_mix_offset = i;
				}
				if (strstr (c->name, "Matrix ") && strstr (c->name, " Mix ")) {
					int last = c->name[strlen (c->name) - 1] - 'A' + 1;
					assert (last > 0 && last <= 20);
					if (last > d.smo) {
						d.smo = last;

						d.matrix_mix_stride = d.smo + 1;
						d.matrix_in_stride = d.smo + 1;
					}
				} else
				if (strstr (c->name, "Mix ") && strstr (c->name, " Input ")) {
					int last = c->name[4] - 'A' + 1;
					assert (last > 0 && last <= 20);
					if (last > d.smo) {
						d.smo = last;

						d.matrix_mix_stride = d.smo + 1;
						d.matrix_in_stride = d.smo + 1;
					}
				}
			}
		}

		if (opts & OPT_PROBE) {
			printf (" %d '%s'", i, c->name);
			if (snd_mixer_selem_is_enumerated (elem)) { printf (", ENUM"); }
			if (snd_mixer_selem_has_playback_switch (elem)) { printf (", PBS"); }
			if (snd_mixer_selem_has_capture_switch (elem)) { printf (", CPS"); }
			printf ("\n");
		}
		++i;
		assert (i <= cnt);
	}

	if ((opts & OPT_DETECT) && rv == 0 && ui->device) {
		if (verbose > 1) {
			printf ("CMP %d\n", memcmp (ui->device, &d, sizeof (Device)));
			dump_device_desc (&d);
			dump_device_desc (ui->device);
		}
		if (d.smi != 0 && d.smo != 0 && d.sin != 0 && d.sout != 0 && d.smst != 0 && d.input_offset != 0 && d.matrix_in_offset != 0 && d.matrix_mix_offset != 0) {
			if (verbose) {
				printf ("Using autodetected mapping.\n");
			}
			memcpy (ui->device, &d, sizeof (Device));
		}
	}
	return rv;
}

static void close_mixer (RobTkApp* ui)
{
	for (unsigned int i = 0; i < ui->ctrl_cnt; ++i) {
		free (ui->ctrl[i].name);
	}
	free (ui->ctrl);
	if (ui->mixer) {
		snd_mixer_close (ui->mixer);
	}
}

static void set_mute (Mctrl* c, bool muted)
{
	int v = muted ? 0 : 1;
	assert (c && snd_mixer_selem_has_playback_switch (c->elem));
	for (int chn = 0; chn <= 2; ++chn) {
		snd_mixer_selem_channel_id_t cid = (snd_mixer_selem_channel_id_t) chn;
		if (snd_mixer_selem_has_playback_channel (c->elem, cid)) {
			snd_mixer_selem_set_playback_switch (c->elem, cid, v);
		}
	}
}

static bool get_mute (Mctrl* c)
{
	int v = 0;
	assert (c && snd_mixer_selem_has_playback_switch (c->elem));
	snd_mixer_selem_get_playback_switch (c->elem, (snd_mixer_selem_channel_id_t)0, &v);
	return v == 0;
}

static float get_dB (Mctrl* c)
{
	assert (c);
	long val = 0;
	snd_mixer_selem_get_playback_dB (c->elem, (snd_mixer_selem_channel_id_t)0, &val);
	return val / 100.f;
}

static void set_dB (Mctrl* c, float dB)
{
	long val = 100.f * dB;
	for (int chn = 0; chn <= 2; ++chn) {
		snd_mixer_selem_channel_id_t cid = (snd_mixer_selem_channel_id_t) chn;
		if (snd_mixer_selem_has_playback_channel (c->elem, cid)) {
			snd_mixer_selem_set_playback_dB (c->elem, cid, val, /*playback*/0);
		}
		if (snd_mixer_selem_has_capture_channel (c->elem, cid)) {
			snd_mixer_selem_set_playback_dB (c->elem, cid, val, /*capture*/1);
		}
	}
}

static float get_dB_range (Mctrl* c, bool maximum)
{
	long min, max;
	min = max = 0;
	snd_mixer_selem_get_playback_dB_range (c->elem, &min, &max);
	if (maximum) {
		return max / 100.f;
	} else {
		return min / 100.f;
	}
}

static void set_enum (Mctrl* c, int v)
{
	assert (snd_mixer_selem_is_enumerated (c->elem));
	snd_mixer_selem_set_enum_item (c->elem, (snd_mixer_selem_channel_id_t)0, v);
}

static int get_enum (Mctrl* c)
{
	unsigned int idx = 0;
	assert (snd_mixer_selem_is_enumerated (c->elem));
	snd_mixer_selem_get_enum_item (c->elem, (snd_mixer_selem_channel_id_t)0, &idx);
	return idx;
}

static void set_switch (Mctrl* c, bool on)
{
	int v = on ? 1 : 0;
	assert (c && snd_mixer_selem_has_capture_switch (c->elem));
	snd_mixer_selem_set_capture_switch (c->elem, 0, v);
}

static bool get_switch (Mctrl* c)
{
	int v = 0;
	assert (c && snd_mixer_selem_has_capture_switch (c->elem));
	snd_mixer_selem_get_capture_switch (c->elem, (snd_mixer_selem_channel_id_t)0, &v);
	return v == 1;
}

/* *****************************************************************************
 * Helpers
 */

static float db_to_knob (float db)
{
	float k = (db + 128.f) / 228.75f;
	float s = k * sqrt (0.5) / (1 - k);
	return s * s;
}

static float knob_to_db (float v)
{
	// v = 0..1
	float db = sqrtf (v) / (sqrtf (0.5) + sqrtf (v)) * 228.75f - 128.f;
	if (db > 6.f) return 6.f;
	return rint (db);
}

/* *****************************************************************************
 * Callbacks
 */

static bool cb_btn_reset (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	/* toggle all values (force change) */

	for (int r = 0; r < ui->device->sin; ++r) {
		Mctrl* sctrl = src_sel (ui, r);
		int mcnt = snd_mixer_selem_get_enum_items (sctrl->elem);
		const int val = robtk_select_get_value (ui->src_sel[r]);
		set_enum (sctrl, (val + 1) % mcnt);
		set_enum (sctrl, val);
	}
	for (int r = 0; r < ui->device->smi; ++r) {
		Mctrl* sctrl = matrix_sel (ui, r);
		int mcnt = snd_mixer_selem_get_enum_items (sctrl->elem);
		const int val = robtk_select_get_value (ui->mtx_sel[r]);
		set_enum (sctrl, (val + 1) % mcnt);
		set_enum (sctrl, val);
	}
	for (unsigned int o = 0; o < ui->device->sout; ++o) {
		Mctrl* sctrl = out_sel (ui, o);
		int mcnt = snd_mixer_selem_get_enum_items (sctrl->elem);
		const int val = robtk_select_get_value (ui->out_sel[o]);
		set_enum (sctrl, (val + 1) % mcnt);
		set_enum (sctrl, val);
	}

	for (int r = 0; r < ui->device->smi; ++r) {
		for (unsigned int c = 0; c < ui->device->smo; ++c) {
			unsigned int n = r * ui->device->smo + c;
			Mctrl* ctrl = matrix_ctrl_cr (ui, c, r);
			const float val = knob_to_db (robtk_dial_get_value (ui->mtx_gain[n]));
			if (val == -128) {
				set_dB (ctrl, 127);
			} else {
				set_dB (ctrl, -128);
			}
			set_dB (ctrl, val);
		}
	}
	for (unsigned int n = 0; n < ui->device->smst; ++n) {
		Mctrl* ctrl = out_gain (ui, n);
		const bool mute = robtk_dial_get_state (ui->out_gain[n]) == 1;
		const float val = knob_to_db (robtk_dial_get_value (ui->out_gain[n]));
		set_mute (ctrl, !mute);
		set_mute (ctrl, mute);
		if (val == -128) {
			set_dB (ctrl, 127);
		} else {
			set_dB (ctrl, -128);
		}
		set_dB (ctrl, val);
	}
	for (unsigned int n = 0; n < ui->device->samo; ++n) {
		Mctrl* ctrl = aux_gain (ui, n);
		const float val = knob_to_db (robtk_dial_get_value (ui->aux_gain[n]));
		if (val == -128) {
			set_dB (ctrl, 127);
		} else {
			set_dB (ctrl, -128);
		}
		set_dB (ctrl, val);
	}
	return TRUE;
}

static bool cb_set_hiz (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	for (uint32_t i = 0; i < ui->device->num_hiz; ++i) {
		int val = robtk_cbtn_get_active (ui->btn_hiz[i]) ? 1 : 0;
		set_enum (hiz (ui, i), val);
	}
	return TRUE;
}

static bool cb_set_pad (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	for (uint32_t i = 0; i < ui->device->num_pad; ++i) {
		if (ui->device->pads_are_switches)
			set_switch (pad (ui, i), robtk_cbtn_get_active (ui->btn_pad[i]));
		else {
			int val = robtk_cbtn_get_active (ui->btn_pad[i]) ? 1 : 0;
			set_enum (pad (ui, i), val);
		}
	}
	return TRUE;
}

static bool cb_set_air (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	for (uint32_t i = 0; i < ui->device->num_air; ++i) {
		set_switch (air (ui, i), robtk_cbtn_get_active (ui->btn_air[i]));
	}
	return TRUE;
}

static bool cb_src_sel (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));
	const float val = robtk_select_get_value (ui->src_sel[n]);
	set_enum (src_sel (ui, n), val);
	return TRUE;
}

static bool cb_mtx_src (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));
	const float val = robtk_select_get_value (ui->mtx_sel[n]);
	set_enum (matrix_sel (ui, n), val);
	return TRUE;
}

static bool cb_mtx_gain (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));
	const float val = knob_to_db (robtk_dial_get_value (ui->mtx_gain[n]));
	if (val == -128) {
		ui->mtx_gain[n]->click_state = 1;
	} else if (val == 0) {
		ui->mtx_gain[n]->click_state = 2;
	} else {
		ui->mtx_gain[n]->click_state = 0;
	}
	if (ui->disable_signals) return TRUE;
	set_dB (matrix_ctrl_n (ui, n), val);
	return TRUE;
}

static bool cb_out_src (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));
	const float val = robtk_select_get_value (ui->out_sel[n]);
	set_enum (out_sel (ui, n), val);
	return TRUE;
}

static bool cb_out_gain (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));
	const bool mute = robtk_dial_get_state (ui->out_gain[n]) == 1;
	const float val = robtk_dial_get_value (ui->out_gain[n]);
	set_mute (out_gain (ui, n), mute);
	set_dB (out_gain (ui, n), knob_to_db (val));
	return TRUE;
}

static bool cb_aux_gain (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	unsigned int n;
	memcpy (&n, w->name, sizeof (unsigned int));
	const float val = robtk_dial_get_value (ui->aux_gain[n]);
	set_dB (aux_gain (ui, n), knob_to_db (val));
	return TRUE;
}

static bool cb_mst_gain (RobWidget* w, void* handle) {
	RobTkApp* ui = (RobTkApp*)handle;
	if (ui->disable_signals) return TRUE;
	const bool mute = robtk_dial_get_state (ui->mst_gain) == 1;
	const float val = robtk_dial_get_value (ui->mst_gain);
	set_mute (mst_gain (ui), mute);
	set_dB (mst_gain (ui), knob_to_db (val));
	return TRUE;
}

/* *****************************************************************************
 * GUI Helpers
 */

static void set_select_values (RobTkSelect* s,  Mctrl* ctrl)
{
	if (!ctrl) return;
	assert (ctrl);

	int mcnt = snd_mixer_selem_get_enum_items (ctrl->elem);
	for (int i = 0; i < mcnt; ++i) {
		char name[64];
		if (snd_mixer_selem_get_enum_item_name (ctrl->elem, i, sizeof (name) - 1, name) < 0) {
			continue;
		}
		robtk_select_add_item (s, i, name);
	}
	robtk_select_set_value (s, get_enum (ctrl));
}

static void dial_annotation_db (RobTkDial* d, cairo_t* cr, void* data)
{
	RobTkApp* ui = (RobTkApp*)data;
	char txt[16];
	snprintf (txt, 16, "%+3.0fdB", knob_to_db (d->cur));

	int tw, th;
	cairo_save (cr);
	PangoLayout * pl = pango_cairo_create_layout (cr);
	pango_layout_set_font_description (pl, ui->font);
	pango_layout_set_text (pl, txt, -1);
	pango_layout_get_pixel_size (pl, &tw, &th);
	cairo_translate (cr, d->w_width / 2, d->w_height - 0);
	cairo_translate (cr, -tw / 2.0 , -th);
	cairo_set_source_rgba (cr, .0, .0, .0, .5);
	rounded_rectangle (cr, -1, -1, tw+3, th+1, 3);
	cairo_fill (cr);
	CairoSetSouerceRGBA (c_wht);
	pango_cairo_show_layout (cr, pl);
	g_object_unref (pl);
	cairo_restore (cr);
	cairo_new_path (cr);
}

static void create_faceplate (RobTkApp *ui) {
	cairo_t* cr;
	float c_bg[4]; get_color_from_theme (1, c_bg);

#define MTX_SF(SF)                                                             \
	SF = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, GD_WIDTH, GED_HEIGHT); \
	cr = cairo_create (SF);                                                      \
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);                              \
	cairo_rectangle (cr, 0, 0, GD_WIDTH, GED_HEIGHT);                            \
	CairoSetSouerceRGBA (c_bg);                                                  \
	cairo_fill (cr);                                                             \
	CairoSetSouerceRGBA (c_g60);                                                 \
	cairo_set_line_cap (cr, CAIRO_LINE_CAP_BUTT);                                \
	cairo_set_line_width (cr, 1.0);

#define MTX_ARROW_H               \
	cairo_move_to (cr, 5, GD_CY);   \
	cairo_rel_line_to (cr, -5, -4); \
	cairo_rel_line_to (cr, 0, 8);   \
	cairo_close_path (cr);          \
	cairo_fill (cr);

#define MTX_ARROW_V                      \
	cairo_move_to (cr, GD_CX, GED_HEIGHT); \
	cairo_rel_line_to (cr, -4, -5);        \
	cairo_rel_line_to (cr, 8, 0);          \
	cairo_close_path (cr);                 \
	cairo_fill (cr);

	MTX_SF (ui->mtx_sf[0]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_WIDTH, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, 0);
	cairo_line_to (cr, GD_CX, GED_HEIGHT);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// top-row
	MTX_SF (ui->mtx_sf[1]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_WIDTH, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, GD_CY);
	cairo_line_to (cr, GD_CX, GED_HEIGHT);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// left column
	MTX_SF (ui->mtx_sf[2]);
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_WIDTH, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, 0);
	cairo_line_to (cr, GD_CX, GED_HEIGHT);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// right column
	MTX_SF (ui->mtx_sf[3]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_CX, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, 0);
	cairo_line_to (cr, GD_CX, GED_HEIGHT);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// top-left
	MTX_SF (ui->mtx_sf[4]);
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_WIDTH, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, GD_CY);
	cairo_line_to (cr, GD_CX, GED_HEIGHT);
	cairo_stroke (cr);
	cairo_destroy (cr);

	// top-right
	MTX_SF (ui->mtx_sf[5]);
	MTX_ARROW_H;
	MTX_ARROW_V;

	cairo_move_to (cr, 0, GD_CY);
	cairo_line_to (cr, GD_CX, GD_CY);
	cairo_stroke (cr);
	cairo_move_to (cr, GD_CX, GD_CY);
	cairo_line_to (cr, GD_CX, GED_HEIGHT);
	cairo_stroke (cr);
	cairo_destroy (cr);
}

static RobWidget* robtk_dial_mouse_intercept (RobWidget* handle, RobTkBtnEvent *ev) {
	RobTkDial* d = (RobTkDial *)GET_HANDLE (handle);
	RobTkApp* ui = (RobTkApp*)d->handle;
	if (!d->sensitive) { return NULL; }

	if (ev->button == 2) {
		/* middle-click exclusively assign output */
		unsigned int n;
		memcpy (&n, d->rw->name, sizeof (unsigned int));

		unsigned c = n % ui->device->smo;
		unsigned r = n / ui->device->smo;
		for (uint32_t i = 0; i < ui->device->smo; ++i) {
			unsigned int nn = r * ui->device->smo + i;
			if (i == c) {
				if (d->cur == 0) {
					robtk_dial_set_value (ui->mtx_gain[nn], db_to_knob (0));
				} else {
					robtk_dial_set_value (ui->mtx_gain[nn], 0);
				}
			} else {
				robtk_dial_set_value (ui->mtx_gain[nn], 0);
			}
		}
		return handle;
	}
	return robtk_dial_mousedown (handle, ev);
}

/* *****************************************************************************
 * GUI
 */

static RobWidget* toplevel (RobTkApp* ui, void* const top) {
	ui->rw = rob_vbox_new (FALSE, 2);
	robwidget_make_toplevel (ui->rw, top);

	create_faceplate (ui);
	ui->font = pango_font_description_from_string ("Mono 9px");

	/* device dependent construction */
	ui->mtx_sel = malloc (ui->device->sin * sizeof (RobTkSelect *));
	ui->mtx_gain = malloc (ui->device->smi * ui->device->smo * sizeof (RobTkDial *));
	ui->mtx_lbl = malloc (ui->device->smo * sizeof (RobTkLbl *));

	ui->src_lbl = malloc (ui->device->sin * sizeof (RobTkLbl *));
	ui->src_sel = malloc (ui->device->sin * sizeof (RobTkSelect *));

	ui->out_lbl = malloc (ui->device->smst * sizeof (RobTkLbl *));
	ui->out_sel = malloc (ui->device->sout * sizeof (RobTkSelect *));
	ui->out_gain = malloc (ui->device->smst * sizeof (RobTkDial *));
	ui->aux_lbl = malloc (ui->device->samo * sizeof (RobTkLbl *));
	ui->aux_gain = malloc (ui->device->samo * sizeof (RobTkDial *));
	ui->sel_lbl = malloc (ui->device->sout - ui->device->samo - (ui->device->smst * 2) * sizeof (RobTkLbl *));


	if (ui->device->num_hiz > 0) {
		ui->btn_hiz = malloc (ui->device->num_hiz * sizeof (RobTkCBtn *));
	} else {
		ui->btn_hiz = NULL;
	}
	if  (ui->device->num_pad > 0) {
		ui->btn_pad = malloc (ui->device->num_pad * sizeof (RobTkCBtn *));
	} else {
		ui->btn_pad = NULL;
	}
	if  (ui->device->num_air > 0) {
		ui->btn_air = malloc (ui->device->num_air * sizeof (RobTkCBtn *));
	} else {
		ui->btn_air = NULL;
	}

	const int c0 = 4; // matrix column offset
	const int rb = 2 + ui->device->smi; // matrix bottom

	/* table layout. NB: these are min sizes, table grows if needed */
	ui->matrix = rob_table_new (/*rows*/rb, /*cols*/ 5 + ui->device->smo, FALSE);
	ui->output = rob_table_new (/*rows*/4,  /*cols*/ 2 + 3 * ui->device->smst, FALSE);

	/* headings */
	ui->heading[0]  = robtk_lbl_new ("Capture");
	rob_table_attach (ui->matrix, robtk_lbl_widget (ui->heading[0]), 2, 3, 0, 1, 2, 6, RTK_EXANDF, RTK_SHRINK);
	ui->heading[1]  = robtk_lbl_new ("Source");
	rob_table_attach (ui->matrix, robtk_lbl_widget (ui->heading[1]), c0, c0 + 1, 0, 1, 2, 6, RTK_SHRINK, RTK_SHRINK);
	ui->heading[2]  = robtk_lbl_new ("Matrix Mixer");
	rob_table_attach (ui->matrix, robtk_lbl_widget (ui->heading[2]), c0 + 1, c0 + 1 + ui->device->smo, 0, 1, 2, 6, RTK_SHRINK, RTK_SHRINK);

	/* input selectors */
	for (unsigned r = 0; r < ui->device->sin; ++r) {
		char txt[8];
		sprintf (txt, "%d", r + 1);
		ui->src_lbl[r] = robtk_lbl_new (txt);
		rob_table_attach (ui->matrix, robtk_lbl_widget (ui->src_lbl[r]), 1, 2, r + 1, r + 2, 2, 2, RTK_SHRINK, RTK_SHRINK);

		ui->src_sel[r] = robtk_select_new ();
		Mctrl* sctrl = src_sel (ui, r);
		int mcnt = snd_mixer_selem_get_enum_items (sctrl->elem);
		set_select_values (ui->src_sel[r], sctrl);
		robtk_select_set_default_item (ui->src_sel[r], src_sel_default (r, mcnt));
		robtk_select_set_callback (ui->src_sel[r], cb_src_sel, ui);

		rob_table_attach (ui->matrix, robtk_select_widget (ui->src_sel[r]), 2, 3, r + 1, r + 2, 2, 2, RTK_SHRINK, RTK_SHRINK);
		// hack alert, abusing the name filed -- should add a .data field to Robwidget
		memcpy (ui->src_sel[r]->rw->name, &r, sizeof (unsigned int));
	}

	/* hidden spacers left/right */
	ui->spc_v[0] = robtk_sep_new (FALSE);
	robtk_sep_set_linewidth (ui->spc_v[0], 0);
	rob_table_attach (ui->matrix, robtk_sep_widget (ui->spc_v[0]), 0, 1, 0, rb, 0, 0, RTK_EXANDF, RTK_FILL);
	ui->spc_v[1] = robtk_sep_new (FALSE);
	robtk_sep_set_linewidth (ui->spc_v[1], 0);
	rob_table_attach (ui->matrix, robtk_sep_widget (ui->spc_v[1]), c0 + 1 + ui->device->smo, c0 + 2 + ui->device->smo, 0, rb, 0, 0, RTK_EXANDF, RTK_FILL);

	/* vertical separator line between inputs and matrix (c0-1 .. c0)*/
	ui->sep_v = robtk_sep_new (FALSE);
	rob_table_attach (ui->matrix, robtk_sep_widget (ui->sep_v), 3, 4, 0, rb, 10, 0, RTK_SHRINK, RTK_FILL);

	/* matrix */
	unsigned int r;

	for (r = 0; r < ui->device->smi; ++r) {
		ui->mtx_sel[r] = robtk_select_new ();

		Mctrl* sctrl = matrix_sel (ui, r);
		set_select_values (ui->mtx_sel[r], sctrl);
		robtk_select_set_default_item (ui->mtx_sel[r], 1 + r); // XXX defaults (0 == off)
		robtk_select_set_callback (ui->mtx_sel[r], cb_mtx_src, ui);

		rob_table_attach (ui->matrix, robtk_select_widget (ui->mtx_sel[r]), c0, c0 + 1, r + 1, r + 2, 2, 2, RTK_SHRINK, RTK_SHRINK);
		memcpy (ui->mtx_sel[r]->rw->name, &r, sizeof (unsigned int));

		for (unsigned int c = 0; c < ui->device->smo; ++c) {
			unsigned int n = r * ui->device->smo + c;
			Mctrl* ctrl = matrix_ctrl_cr (ui, c, r);
			assert (ctrl);
			ui->mtx_gain[n] = robtk_dial_new_with_size (
					0, 1, 1.f / 80.f,
					GD_WIDTH, GED_HEIGHT, GD_CX, GD_CY, GED_RADIUS);
			robtk_dial_set_default (ui->mtx_gain[n], db_to_knob (0));
			robtk_dial_set_value (ui->mtx_gain[n], db_to_knob (get_dB (ctrl)));
			robtk_dial_set_callback (ui->mtx_gain[n], cb_mtx_gain, ui);
			robtk_dial_annotation_callback (ui->mtx_gain[n], dial_annotation_db, ui);
			robwidget_set_mousedown (ui->mtx_gain[n]->rw, robtk_dial_mouse_intercept);
			ui->mtx_gain[n]->displaymode = 3;

			if (0 == robtk_dial_get_value (ui->mtx_gain[n])) {
				ui->mtx_gain[n]->click_state = 1;
			}
			else if (0 == knob_to_db (robtk_dial_get_value (ui->mtx_gain[n]))) {
				ui->mtx_gain[n]->click_state = 2;
			}

			if (c == (ui->device->smo - 1) && r == 0) {
				robtk_dial_set_surface (ui->mtx_gain[n], ui->mtx_sf[5]);
			}
			else if (c == 0 && r == 0) {
				robtk_dial_set_surface (ui->mtx_gain[n], ui->mtx_sf[4]);
			}
			else if (c == (ui->device->smo - 1)) {
				robtk_dial_set_surface (ui->mtx_gain[n], ui->mtx_sf[3]);
			}
			else if (c == 0) {
				robtk_dial_set_surface (ui->mtx_gain[n], ui->mtx_sf[2]);
			}
			else if (r == 0) {
				robtk_dial_set_surface (ui->mtx_gain[n], ui->mtx_sf[1]);
			}
			else {
				robtk_dial_set_surface (ui->mtx_gain[n], ui->mtx_sf[0]);
			}

			rob_table_attach (ui->matrix, robtk_dial_widget (ui->mtx_gain[n]), c0 + c + 1, c0 + c + 2, r + 1, r + 2, 0, 0, RTK_SHRINK, RTK_SHRINK);

			memcpy (ui->mtx_gain[n]->rw->name, &n, sizeof (unsigned int));
		}
	}

	/* matrix out labels */
	for (unsigned int c = 0; c < ui->device->smo; ++c) {
		char txt[8];
		sprintf (txt, "Mix %c", 'A' + c);
		ui->mtx_lbl[c]  = robtk_lbl_new (txt);
		rob_table_attach (ui->matrix, robtk_lbl_widget (ui->mtx_lbl[c]), c0 + c + 1, c0 + c + 2, r + 1, r + 2, 2, 2, RTK_SHRINK, RTK_SHRINK);
	}

	/*** output Table ***/

	/* master level */
	if (ui->device->smst) {
	ui->out_mst = robtk_lbl_new ("Master");
	rob_table_attach (ui->output, robtk_lbl_widget (ui->out_mst), 0, 2, 0, 1, 2, 2, RTK_SHRINK, RTK_SHRINK);
	{
		Mctrl* ctrl = mst_gain (ui);
		ui->mst_gain = robtk_dial_new_with_size (
				0, 1, 1.f / 80.f,
				75, 50, 37.5, 22.5, 20);

		robtk_dial_enable_states (ui->mst_gain, 1);
		robtk_dial_set_state_color (ui->mst_gain, 1, .5, .2, .2, 1.0);

		robtk_dial_set_default (ui->mst_gain, db_to_knob (0));
		robtk_dial_set_default_state (ui->mst_gain, 0);

		robtk_dial_set_value (ui->mst_gain, db_to_knob (get_dB (ctrl)));
		robtk_dial_set_state (ui->mst_gain, get_mute (ctrl) ? 1 : 0);
		robtk_dial_set_callback (ui->mst_gain, cb_mst_gain, ui);
		robtk_dial_annotation_callback (ui->mst_gain, dial_annotation_db, ui);
		rob_table_attach (ui->output, robtk_dial_widget (ui->mst_gain), 0, 2, 1, 3, 2, 0, RTK_SHRINK, RTK_SHRINK);
	}
	}

	/* output level + labels */
	for (unsigned int o = 0; o < ui->device->smst; ++o) {
		int row = 4 * floor (o / 5); // beware of bleed into Hi-Z, Pads
		int oc = o % 5;

		ui->out_lbl[o]  = robtk_lbl_new (out_gain_label (ui, o));
		rob_table_attach (ui->output, robtk_lbl_widget (ui->out_lbl[o]), 3 * oc + 2, 3 * oc + 5, row, row + 1, 2, 2, RTK_SHRINK, RTK_SHRINK);

		Mctrl* ctrl = out_gain (ui, o);
		ui->out_gain[o] = robtk_dial_new_with_size (
				0, 1, 1.f / 80.f,
				65, 40, 32.5, 17.5, 15);

		robtk_dial_enable_states (ui->out_gain[o], 1);
		robtk_dial_set_state_color (ui->out_gain[o], 1, .5, .3, .1, 1.0);

		robtk_dial_set_default (ui->out_gain[o], db_to_knob (0));
		robtk_dial_set_default_state (ui->out_gain[o], 0);

		robtk_dial_set_value (ui->out_gain[o], db_to_knob (get_dB (ctrl)));
		robtk_dial_set_state (ui->out_gain[o], get_mute (ctrl) ? 1 : 0);
		robtk_dial_set_callback (ui->out_gain[o], cb_out_gain, ui);
		robtk_dial_annotation_callback (ui->out_gain[o], dial_annotation_db, ui);
		rob_table_attach (ui->output, robtk_dial_widget (ui->out_gain[o]), 3 * oc + 2, 3 * oc + 5, row + 1, row + 2, 2, 0, RTK_SHRINK, RTK_SHRINK);

		memcpy (ui->out_gain[o]->rw->name, &o, sizeof (unsigned int));
	}

	/* aux mono outputs & labels */
	for (unsigned int o = 0; o < ui->device->samo; ++o) {
		int row = 4 * floor (o / 5); // beware of bleed into Hi-Z, Pads
		int oc = o % 5;

		ui->aux_lbl[o]  = robtk_lbl_new (aux_gain_label (ui, o));
		rob_table_attach (ui->output, robtk_lbl_widget (ui->aux_lbl[o]), 3 * oc + 2, 3 * oc + 5, row, row + 1, 2, 2, RTK_SHRINK, RTK_SHRINK);

		Mctrl* ctrl = aux_gain (ui, o);
		ui->aux_gain[o] = robtk_dial_new_with_size (
				0, 1, 1.f / 80.f,
				65, 40, 32.5, 17.5, 15);

		robtk_dial_enable_states (ui->aux_gain[o], 1);
		robtk_dial_set_state_color (ui->aux_gain[o], 1, .5, .3, .1, 1.0);

		robtk_dial_set_default (ui->aux_gain[o], db_to_knob (0));
		robtk_dial_set_default_state (ui->aux_gain[o], 0);

		robtk_dial_set_value (ui->aux_gain[o], db_to_knob (get_dB (ctrl)));
		robtk_dial_set_callback (ui->aux_gain[o], cb_aux_gain, ui);
		robtk_dial_annotation_callback (ui->aux_gain[o], dial_annotation_db, ui);
		rob_table_attach (ui->output, robtk_dial_widget (ui->aux_gain[o]), 3 * oc + 2, 3 * oc + 5, row + 1, row + 2, 2, 0, RTK_SHRINK, RTK_SHRINK);

		memcpy (ui->aux_gain[o]->rw->name, &o, sizeof (unsigned int));
	}

	for (unsigned int o = 0; o < ui->device->sout - ui->device->samo - (ui->device->smst * 2); ++o) {
		int row_base = (o + ui->device->samo + (ui->device->smst * 2));
		int row = 4 * floor (row_base / 6); // beware of bleed into Hi-Z, Pads
		int oc = row_base % 6;

		ui->sel_lbl[o]  = robtk_lbl_new (out_select_label (ui, o));
		rob_table_attach (ui->output, robtk_lbl_widget (ui->sel_lbl[o]), 3 * oc + 2, 3 * oc + 5, row, row + 1, 2, 2, RTK_SHRINK, RTK_SHRINK);
	}

	/* Hi-Z*/
	for (unsigned int i = 0; i < ui->device->num_hiz; ++i) {
		ui->btn_hiz[i] = robtk_cbtn_new ("HiZ", GBT_LED_LEFT, false);
		robtk_cbtn_set_active (ui->btn_hiz[i], get_enum (hiz (ui, i)) == 1);
		robtk_cbtn_set_callback (ui->btn_hiz[i], cb_set_hiz, ui);
		rob_table_attach (ui->output, robtk_cbtn_widget (ui->btn_hiz[i]),
				i, i + 1, 3, 4, 0, 0, RTK_SHRINK, RTK_SHRINK);
	}

	/* Pads */
	for (unsigned int i = 0; i < ui->device->num_pad; ++i) {
		ui->btn_pad[i] = robtk_cbtn_new ("Pad", GBT_LED_LEFT, false);
		if (ui->device->pads_are_switches)
			robtk_cbtn_set_active (ui->btn_pad[i], get_switch (pad (ui, i)) == 1);
		else
			robtk_cbtn_set_active (ui->btn_pad[i], get_enum (pad (ui, i)) == 1);
		robtk_cbtn_set_callback (ui->btn_pad[i], cb_set_pad, ui);
		rob_table_attach (ui->output, robtk_cbtn_widget (ui->btn_pad[i]),
				i, i + 1, 4, 5, 0, 0, RTK_SHRINK, RTK_SHRINK);
	}

	/* Airs */
	for (unsigned int i = 0; i < ui->device->num_air; ++i) {
		ui->btn_air[i] = robtk_cbtn_new ("Air", GBT_LED_LEFT, false);
			robtk_cbtn_set_active (ui->btn_air[i], get_switch (air (ui, i)) == 1);
		robtk_cbtn_set_callback (ui->btn_air[i], cb_set_air, ui);
		rob_table_attach (ui->output, robtk_cbtn_widget (ui->btn_air[i]),
				i, i + 1, 5, 6, 0, 0, RTK_SHRINK, RTK_SHRINK);
	}

	/* output selectors */
	for (unsigned int o = 0; o < ui->device->sout; ++o) {
		int row = 4 * floor (o / 10); // beware of bleed into Hi-Z, Pads
		int pc = 3 * (o / 2); /* stereo-pair column */
		pc %= 15;

		ui->out_sel[o] = robtk_select_new ();
		Mctrl* sctrl = out_sel (ui, o);
		set_select_values (ui->out_sel[o], sctrl);
		robtk_select_set_default_item (ui->out_sel[o], out_sel_default (o));
		robtk_select_set_callback (ui->out_sel[o], cb_out_src, ui);

		memcpy (ui->out_sel[o]->rw->name, &o, sizeof (unsigned int));

		if (o < (ui->device->smst * 2)) {
			if (o & 1) {
				/* right channel */
				rob_table_attach (ui->output, robtk_select_widget (ui->out_sel[o]), 3 + pc, 5 + pc, row + 3, row + 4, 2, 2, RTK_SHRINK, RTK_SHRINK);
			} else {
				/* left channel */
				rob_table_attach (ui->output, robtk_select_widget (ui->out_sel[o]), 2 + pc, 4 + pc, row + 2, row + 3, 2, 2, RTK_SHRINK, RTK_SHRINK);
			}
		} else {
			/* mono channel */
			pc = 3 * o;

			rob_table_attach (ui->output, robtk_select_widget (ui->out_sel[o]), 2 + pc, 5 + pc, row + 3, row + 4, 2, 2, RTK_SHRINK, RTK_SHRINK);
		}
	}

#if 0
	/* re-send */
	ui->btn_reset = robtk_pbtn_new ("R");
	rob_table_attach (ui->output, robtk_pbtn_widget (ui->btn_reset), 1 + 3 * (ui->device->sout / 2), 2 + 3 * (ui->device->sout / 2), 2, 3, 2, 2, RTK_SHRINK, RTK_SHRINK);
	robtk_pbtn_set_callback_up (ui->btn_reset, cb_btn_reset, ui);
#endif

	ui->sep_h = robtk_sep_new (TRUE);

	/* top-level packing */
	rob_vbox_child_pack (ui->rw, ui->matrix, TRUE, TRUE);
	rob_vbox_child_pack (ui->rw, robtk_sep_widget (ui->sep_h), TRUE, TRUE);
	rob_vbox_child_pack (ui->rw, ui->output, TRUE, TRUE);
	return ui->rw;
}

static void gui_cleanup (RobTkApp* ui) {

	close_mixer (ui);
	free (ui->pollfds);

	for (int i = 0; i < ui->device->sin; ++i) {
		robtk_select_destroy (ui->src_sel[i]);
		robtk_lbl_destroy (ui->src_lbl[i]);
	}
	for (int r = 0; r < ui->device->smi; ++r) {
		robtk_select_destroy (ui->mtx_sel[r]);
		for (int c = 0; c < ui->device->smo; ++c) {
			robtk_dial_destroy (ui->mtx_gain[r * ui->device->smo + c]);
		}
	}
	for (int i = 0; i < ui->device->smo; ++i) {
		robtk_lbl_destroy (ui->mtx_lbl[i]);
	}
	for (int i = 0; i < ui->device->sout; ++i) {
		robtk_select_destroy (ui->out_sel[i]);
	}
	for (int i = 0; i < ui->device->smst; ++i) {
		robtk_lbl_destroy (ui->out_lbl[i]);
		robtk_dial_destroy (ui->out_gain[i]);
	}

	for (int i = 0; i < 3; ++i) {
		robtk_lbl_destroy (ui->heading[i]);
	}
	for (int i = 0; i < 6; ++i) {
		cairo_surface_destroy (ui->mtx_sf[i]);
	}

	if (ui->device->smst) {
		robtk_lbl_destroy (ui->out_mst);
		robtk_dial_destroy (ui->mst_gain);
	}

	for (int i = 0; i < ui->device->num_hiz; i++) {
		robtk_cbtn_destroy (ui->btn_hiz[i]);
	}

	for (int i = 0; i < ui->device->num_pad; i++) {
		robtk_cbtn_destroy (ui->btn_pad[i]);
	}

	for (int i = 0; i < ui->device->num_air; i++) {
		robtk_cbtn_destroy (ui->btn_air[i]);
	}

	robtk_sep_destroy (ui->sep_v);
	robtk_sep_destroy (ui->sep_h);
	robtk_sep_destroy (ui->spc_v[0]);
	robtk_sep_destroy (ui->spc_v[1]);

	rob_table_destroy (ui->output);
	rob_table_destroy (ui->matrix);
	rob_box_destroy (ui->rw);

	pango_font_description_free (ui->font);

	free (ui->mtx_sel);
	free (ui->mtx_gain);
	free (ui->mtx_lbl);

	free (ui->src_lbl);
	free (ui->src_sel);

	free (ui->out_lbl);
	free (ui->out_sel);
	free (ui->out_gain);

	free (ui->aux_gain);
	free (ui->aux_lbl);
	free (ui->sel_lbl);

	free (ui->btn_hiz);
	free (ui->btn_pad);
	free (ui->btn_air);
}

static char* lookup_device ()
{
	char* card = NULL;
	snd_ctl_card_info_t* info;
	snd_ctl_card_info_alloca(&info);
	int number = -1;
	while (!card) {
		int err = snd_card_next(&number);
		if (err < 0 || number < 0) {
			break;
		}
		snd_ctl_t* ctl;
		char buf[16];
		sprintf (buf, "hw:%d", number);
		err = snd_ctl_open(&ctl, buf, 0);
		if (err < 0) {
			continue;
		}
		err = snd_ctl_card_info(ctl, info);
		snd_ctl_close(ctl);
		if (err < 0) {
			continue;
		}
		const char* card_name = snd_ctl_card_info_get_name (info);
		if (!card_name) {
			continue;
		}
		if (verbose > 1) {
			printf ("* hw:%d \"%s\"\n", number, card_name);
		}
		for (unsigned i = 0; i < NUM_DEVICES; i++) {
			if (!strcmp (card_name, devices[i].name)) {
				card = strdup (buf);
			}
		}
	}
	if (verbose > 0 && NULL != card) {
		printf ("Autodetect: Using \"%s\"\n", card);
	}
	return card;
}

/* *****************************************************************************
 * options + help
 */

static struct option const long_options[] =
{
	{"help", no_argument, 0, 'h'},
	{"preset-only", no_argument, 0, 'P'},
	{"print-controls", no_argument, 0, 'p'},
	{"version", no_argument, 0, 'V'},
	{"verbose", no_argument, 0, 'v'},
	{NULL, 0, NULL, 0}
};

static void usage (int status) {
	printf ("scarlett-mixer - Mixer GUI for Focusrite Scarlett USB Devices.\n\n\
A graphical audio-mixer user-interface that exposes the direct raw controls of\n\
the hardware mixer in the Focusrite(R)-Scarlett(TM) Series of USB soundcards.\n\
\n\
Unless specified on the commandline, the tool uses the first supported device\n\
falling back to '%s'.\n\
\n\
Supported devices:\n\
", DEFAULT_DEVICE);

	for (unsigned i = 0; i < NUM_DEVICES; i++) {
		printf ("* %s\n", devices[i].name);
	}

	printf ("Usage: scarlett-mixer [ OPTIONS ] [ DEVICE ]\n\n");
	printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -p, --print-controls       list control parameters of given soundcard\n\
  -P, --preset-only          do not parse names from kernel-driver\n\
  -V, --version              print version information and exit\n\
  -v, --verbose              print information (may be specifified twice)\n\
\n\n\
Examples:\n\
scarlett-mixer hw:1\n\
\n");
	printf ("Report bugs to <https://github.com/x42/scarlett-mixer/issues>\n");
	exit (status);
}


/* *****************************************************************************
 * RobTk-app (LV2 wrapper)
 */

#define LVGL_RESIZEABLE

static void ui_enable (LV2UI_Handle handle) { }
static void ui_disable (LV2UI_Handle handle) { }

static LV2UI_Handle
instantiate (
		void* const               ui_toplevel,
		const LV2UI_Descriptor*   descriptor,
		const char*               plugin_uri,
		const char*               bundle_path,
		LV2UI_Write_Function      write_function,
		LV2UI_Controller          controller,
		RobWidget**               widget,
		const LV2_Feature* const* features)
{
	RobTkApp* ui = (RobTkApp*) calloc (1,sizeof (RobTkApp));
	char* card = NULL;

	struct _rtkargv { int argc; char **argv; };
	struct _rtkargv* rtkargv = NULL;

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, "http://gareus.org/oss/lv2/robtk#argv")) {
			rtkargv = (struct _rtkargv*)features[i]->data;
		}
	}

	int opts = OPT_DETECT;
	int c;
	while (rtkargv && (c = getopt_long (rtkargv->argc, rtkargv->argv,
			   "h"  /* help */
			   "P"  /* Preset-Only */
			   "p"  /* print-controls */
			   "V"  /* version */
			   "v", /* verbose */
			   long_options, (int *) 0)) != EOF) {
		switch (c) {
			case 'h':
				usage (0);
			case 'V':
				printf ("scarlet-mixer version %s\n\n", VERSION);
				printf ("Copyright (C) GPL 2019 Robin Gareus <robin@gareus.org>\n");
				exit (0);
			case 'v':
				++verbose;
				break;
			case 'P':
				opts &= ~OPT_DETECT;
				break;
			case 'p':
				opts |= OPT_PROBE;
				break;
			default:
				usage (EXIT_FAILURE);
		}
	}

	if (rtkargv && rtkargv->argc > optind + 1) {
		usage (EXIT_FAILURE);
	}

	if (rtkargv && rtkargv->argc > optind) {
		card = strdup (rtkargv->argv[optind]);
	}
	if (!card) {
		card = lookup_device ();
	}
	if (!card) {
		card = strdup (DEFAULT_DEVICE);
	}

	if (open_mixer (ui, card, opts)) {
		close_mixer (ui);
		free (ui);
		free (card);
		return 0;
	}
	ui->disable_signals = true;
	*widget = toplevel (ui, ui_toplevel);
	ui->disable_signals = false;
	free (card);
	return ui;
}

static enum LVGLResize
plugin_scale_mode (LV2UI_Handle handle)
{
	return LVGL_LAYOUT_TO_FIT;
}

static void
cleanup (LV2UI_Handle handle)
{
	RobTkApp* ui = (RobTkApp*)handle;
	gui_cleanup (ui);
	free (ui);
}

static const void*
extension_data (const char* uri)
{
	return NULL;
}

static void
port_event (LV2UI_Handle handle,
            uint32_t     port_index,
            uint32_t     buffer_size,
            uint32_t     format,
            const void*  buffer)
{
	RobTkApp* ui = (RobTkApp*)handle;
	assert (ui->mixer);

	int n = snd_mixer_poll_descriptors_count (ui->mixer);
	unsigned short revents;

	if (n != ui->nfds) {
		free (ui->pollfds);
		ui->nfds = n;
		ui->pollfds = (struct pollfd*)calloc (n, sizeof (struct pollfd));
	}
	if (snd_mixer_poll_descriptors (ui->mixer, ui->pollfds, n) < 0) {
		return;
	}
	n = poll (ui->pollfds, ui->nfds, 0);
	if (n <= 0) {
		return;
	}

	if (snd_mixer_poll_descriptors_revents (ui->mixer, ui->pollfds, n, &revents) < 0) {
		fprintf (stderr, "cannot get poll events\n");
		robtk_close_self (ui->rw->top);
	}
	if (revents & (POLLERR | POLLNVAL)) {
		fprintf (stderr, "Poll error\n");
		robtk_close_self (ui->rw->top);
	}
	else if (revents & POLLIN) {
		snd_mixer_handle_events (ui->mixer);
	}

	/* simply update the complete GUI (on any change) */

	ui->disable_signals = true;
	Mctrl* ctrl;

	for (unsigned int r = 0; r < ui->device->sin; ++r) {
		ctrl = src_sel (ui, r);
		robtk_select_set_value (ui->src_sel[r], get_enum (ctrl));
	}

	for (unsigned int r = 0; r < ui->device->smi; ++r) {
		ctrl = matrix_sel (ui, r);
		robtk_select_set_value (ui->mtx_sel[r], get_enum (ctrl));

		for (unsigned int c = 0; c < ui->device->smo; ++c) {
			unsigned int n = r * ui->device->smo + c;
			ctrl = matrix_ctrl_cr (ui, c, r);
			robtk_dial_set_value (ui->mtx_gain[n], db_to_knob (get_dB (ctrl)));
		}
	}

	for (unsigned int o = 0; o < ui->device->smst; ++o) {
		ctrl = out_gain (ui, o);
		robtk_dial_set_value (ui->out_gain[o], db_to_knob (get_dB (ctrl)));
		robtk_dial_set_state (ui->out_gain[o], get_mute (ctrl) ? 1 : 0);
	}

	for (unsigned int o = 0; o < ui->device->samo; ++o) {
		ctrl = aux_gain (ui, o);
		robtk_dial_set_value (ui->aux_gain[o], db_to_knob (get_dB (ctrl)));
	}

	if (ui->device->smst) {
		ctrl = mst_gain (ui);
		robtk_dial_set_value (ui->mst_gain, db_to_knob (get_dB (ctrl)));
		robtk_dial_set_state (ui->mst_gain, get_mute (ctrl) ? 1 : 0);
	}

	for (unsigned int i = 0; i < ui->device->num_hiz; ++i) {
		robtk_cbtn_set_active (ui->btn_hiz[i], get_enum (hiz (ui, i)) == 1);
	}

	for (unsigned int o = 0; o < ui->device->sout; ++o) {
		ctrl = out_sel (ui, o);
		robtk_select_set_value (ui->out_sel[o], get_enum (ctrl));
	}

	ui->disable_signals = false;
}
