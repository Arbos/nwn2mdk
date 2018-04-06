/**
* Derived from https://github.com/berenm/xoreos-tools/blob/wip/granny-decoder/src/decompress.cpp
*
* Distributed under the Boost Software License, Version 1.0.
* See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt
*/

#include <algorithm>
#include <numeric>
#include <vector>
#include <cstring>

#include "gr2_decompress.h"

struct parameters;

struct decoder {
	uint32_t numer;
	uint32_t denom;
	uint32_t next_denom;
	uint8_t* stream;

	decoder(uint8_t* stream);

	uint16_t decode(uint16_t max);
	uint16_t commit(uint16_t max, uint16_t val, uint16_t err);
	uint16_t decode_and_commit(uint16_t max);
};

struct weighwindow {
	uint16_t count_cap;

	std::vector< uint16_t > ranges;
	std::vector< uint16_t > values;
	std::vector< uint16_t > weights;
	uint16_t                weight_total;

	uint16_t thresh_increase;
	uint16_t thresh_increase_cap;
	uint16_t thresh_range_rebuild;
	uint16_t thresh_weight_rebuild;

	weighwindow(uint32_t max_value, uint16_t count_cap);

	void rebuild_weights();
	void rebuild_ranges();
	auto try_decode(decoder & dec);
};

struct dictionary {
	uint32_t decoded_size;
	uint32_t backref_size;

	uint32_t decoded_value_max;
	uint32_t backref_value_max;
	uint32_t lowbit_value_max;
	uint32_t midbit_value_max;
	uint32_t highbit_value_max;

	weighwindow                lowbit_window;
	weighwindow                highbit_window;
	std::vector< weighwindow > midbit_windows;

	std::vector< weighwindow > decoded_windows;
	std::vector< weighwindow > size_windows;

	dictionary(parameters& params);

	uint32_t decompress_block(decoder& dec, uint8_t* dbuf);
};

struct parameters {
	unsigned decoded_value_max : 9;
	unsigned backref_value_max : 23;
	unsigned decoded_count : 9;
	unsigned padding : 10;
	unsigned highbit_count : 13;
	uint8_t  sizes_count[4];
};

static_assert(sizeof(parameters) == 12);

decoder::decoder(uint8_t* stream) {
	this->numer = stream[0] >> 1;
	this->denom = 0x80;
	this->stream = stream;
}

uint16_t decoder::decode(uint16_t max) {
	for (; this->denom <= 0x800000; this->denom <<= 8) {
		this->numer <<= 8;
		this->numer |= (this->stream[0] << 7) & 0x80;
		this->numer |= (this->stream[1] >> 1) & 0x7f;
		this->stream++;
	}

	this->next_denom = this->denom / max;
	return std::min(this->numer / this->next_denom, max - 1u);
}

uint16_t decoder::commit(uint16_t max, uint16_t val, uint16_t err) {
	this->numer -= this->next_denom * val;

	if (val + err < max)
		this->denom = this->next_denom * err;
	else
		this->denom -= this->next_denom * val;

	return val;
}

uint16_t decoder::decode_and_commit(uint16_t max) {
	return this->commit(max, this->decode(max), 1);
}

weighwindow::weighwindow(uint32_t max_value, uint16_t count_cap) {
	this->weight_total = 4;
	this->count_cap = count_cap + 1;

	this->ranges.emplace_back(0);
	this->ranges.emplace_back(0x4000);

	this->weights.emplace_back(4);
	this->values.emplace_back(0);

	this->thresh_increase = 4;
	this->thresh_range_rebuild = 8;
	this->thresh_weight_rebuild = std::max(256u, std::min(32 * max_value, 15160u));

	if (max_value > 64)
		this->thresh_increase_cap = std::min(2 * max_value, this->thresh_weight_rebuild / 2 - 32u);
	else
		this->thresh_increase_cap = 128;
}

void weighwindow::rebuild_ranges() {
	this->ranges.resize(this->weights.size());

	auto range_weight = 8 * 0x4000 / this->weight_total;
	auto range_start = 0;
	for (size_t i = 0; i < this->weights.size(); ++i) {
		this->ranges[i] = range_start;
		range_start += (this->weights[i] * range_weight) / 8;
	}
	this->ranges.emplace_back(0x4000);

	if (this->thresh_increase > this->thresh_increase_cap / 2) {
		this->thresh_range_rebuild = this->weight_total + this->thresh_increase_cap;
	}
	else {
		this->thresh_increase *= 2;
		this->thresh_range_rebuild = this->weight_total + this->thresh_increase;
	}
}

void weighwindow::rebuild_weights() {
	std::transform(std::begin(this->weights),
		std::end(this->weights),
		std::begin(this->weights),
		[](uint16_t& w) { return w / 2; });

	this->weight_total = std::accumulate(std::begin(this->weights), std::end(this->weights), 0);

	for (uint32_t i = 1; i < this->weights.size(); i++) {
		while (i < this->weights.size() && this->weights[i] == 0) {
			std::swap(this->weights[i], this->weights.back());
			std::swap(this->values[i], this->values.back());

			this->weights.pop_back();
			this->values.pop_back();
		}
	}

	auto it = std::max_element(std::begin(this->weights) + 1, std::end(this->weights));
	if (it != std::end(this->weights)) {
		auto const i = std::distance(std::begin(this->weights), it);
		std::swap(this->weights[i], this->weights.back());
		std::swap(this->values[i], this->values.back());
	}

	if ((this->weights.size() < this->count_cap) && (this->weights[0] == 0)) {
		this->weights[0] = 1;
		this->weight_total++;
	}
}

auto weighwindow::try_decode(decoder & dec) {
	if (this->weight_total >= this->thresh_range_rebuild) {
		if (this->thresh_range_rebuild >= this->thresh_weight_rebuild)
			this->rebuild_weights();
		this->rebuild_ranges();
	}

	auto value = dec.decode(0x4000);
	auto rangeit = std::upper_bound(std::begin(this->ranges), std::end(this->ranges), value) - 1;
	dec.commit(0x4000, *rangeit, *std::next(rangeit) - *rangeit);

	auto index = std::distance(std::begin(this->ranges), rangeit);
	this->weights[index]++;
	this->weight_total++;

	if (index > 0)
		return std::make_pair((uint16_t*) nullptr, this->values[index]);

	if ((this->weights.size() >= this->ranges.size())
		&& (dec.decode_and_commit(2) == 1)) {
		auto index = this->ranges.size() + dec.decode_and_commit(this->weights.size() - this->ranges.size() + 1) - 1u;

		this->weights[index] += 2;
		this->weight_total += 2;

		return std::make_pair((uint16_t*) nullptr, this->values[index]);
	}

	this->values.emplace_back(0);
	this->weights.emplace_back(2);
	this->weight_total += 2;

	if (this->weights.size() == this->count_cap) {
		this->weight_total -= this->weights[0];
		this->weights[0] = 0;
	}

	return std::make_pair(&this->values.back(), (uint16_t)0);
}

dictionary::dictionary(parameters& params) :
	decoded_size(0),
	backref_size(0),

	decoded_value_max(params.decoded_value_max),
	backref_value_max(params.backref_value_max),
	lowbit_value_max(std::min(backref_value_max + 1, 4u)),
	midbit_value_max(std::min(backref_value_max / 4 + 1, 256u)),
	highbit_value_max(backref_value_max / 1024u + 1),

	lowbit_window(lowbit_value_max - 1, lowbit_value_max),
	highbit_window(highbit_value_max - 1, params.highbit_count + 1) {

	for (size_t i = 0; i < this->highbit_value_max; ++i) {
		this->midbit_windows.emplace_back(this->midbit_value_max - 1, this->midbit_value_max);
	}

	for (size_t i = 0; i < 4; ++i) {
		this->decoded_windows.emplace_back(this->decoded_value_max - 1, (uint32_t)params.decoded_count);
	}

	for (size_t i = 0; i < 4; ++i) {
		for (size_t j = 0; j < 16; ++j) {
			this->size_windows.emplace_back(64, params.sizes_count[3 - i]);
		}
	}
	this->size_windows.emplace_back(64, params.sizes_count[0]);
}

uint32_t dictionary::decompress_block(decoder& dec, uint8_t* dbuf) {
	auto d1 = this->size_windows[this->backref_size].try_decode(dec);

	if (d1.first)
		d1.second = (*d1.first = dec.decode_and_commit(65));
	this->backref_size = d1.second;

	if (this->backref_size > 0) {
		static uint32_t const sizes[] = { 128u, 192u, 256u, 512u };

		auto backref_size = this->backref_size < 61u ? this->backref_size + 1 : sizes[this->backref_size - 61u];
		auto backref_range = std::min(this->backref_value_max, this->decoded_size);

		auto d3 = this->lowbit_window.try_decode(dec);
		if (d3.first)
			d3.second = (*d3.first = dec.decode_and_commit(this->lowbit_value_max));

		auto d4 = this->highbit_window.try_decode(dec);
		if (d4.first)
			d4.second = (*d4.first = dec.decode_and_commit(backref_range / 1024u + 1));

		auto d5 = this->midbit_windows[d4.second].try_decode(dec);
		if (d5.first)
			d5.second = (*d5.first = dec.decode_and_commit(std::min(backref_range / 4 + 1, 256u)));

		auto backref_offset = (d4.second << 10) + (d5.second << 2) + d3.second + 1u;

		this->decoded_size += backref_size;

		size_t repeat = backref_size / backref_offset;
		size_t remain = backref_size % backref_offset;
		for (size_t i = 0; i < repeat; ++i) {
			std::memcpy(dbuf + i * backref_offset, dbuf - backref_offset, backref_offset);
		}
		std::memcpy(dbuf + repeat * backref_offset, dbuf - backref_offset, remain);

		return backref_size;
	}
	else {
		auto i = (uintptr_t)dbuf % 4;
		auto d2 = this->decoded_windows[i].try_decode(dec);
		if (d2.first)
			d2.second = (*d2.first = dec.decode_and_commit(this->decoded_value_max));

		dbuf[0] = d2.second & 0xff;
		this->decoded_size++;

		return 1;
	}
}

void gr2_decompress(uint32_t csize, uint8_t* cbuf,
	uint32_t step1, uint32_t step2,
	uint32_t dsize, uint8_t* dbuf)
{
	if (csize == 0)
		return;

	std::memset(cbuf + csize, 0, (4 - csize) % 4);

	parameters params[3] = {};
	std::memcpy(params, cbuf, sizeof(params));

	decoder  dec = decoder(cbuf + sizeof(params));
	uint32_t steps[] = { step1, step2, dsize };
	uint8_t* dptr = dbuf;

	for (uint32_t i = 0; i < 3; ++i) {
		dictionary dic(params[i]);

		while (dptr < dbuf + steps[i]) {
			dptr += dic.decompress_block(dec, dptr);
		}
	}
}
