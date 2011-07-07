/*
 * 2011+ Copyright (c) Evgeniy Polyakov <zbr@ioremap.net>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <sys/types.h>

#include <unistd.h>
#include <string.h>

#include <eblob/eblob.hpp>

using namespace zbr;

eblob::eblob(const char *log_file, const unsigned int log_mask, const std::string &eblob_path) :
	logger_(log_file, log_mask)
{
	std::ostringstream mstr;
	mstr << eblob_path << ".mmap";

	std::string mmap_file = mstr.str();

	struct eblob_config cfg;

	memset(&cfg, 0, sizeof(cfg));
	cfg.file = (char *)eblob_path.c_str();
	cfg.mmap_file = (char *)mmap_file.c_str();
	cfg.log = logger_.log();
	cfg.iterate_threads = 16;
	cfg.sync = 30;

	eblob_ = eblob_init(&cfg);
	if (!eblob_) {
		throw std::runtime_error("Failed to initialize eblob");
	}
}

eblob::eblob(struct eblob_config *cfg) : logger_(NULL, 0)
{
	eblob_ = eblob_init(cfg);
	if (!eblob_) {
		throw std::runtime_error("Failed to initialize eblob");
	}
}

eblob::~eblob()
{
	eblob_cleanup(eblob_);
}

void eblob::write(const struct eblob_key &key, const void *data, const uint64_t dsize, uint64_t flags, int type)
{
	int err = eblob_write(eblob_, (struct eblob_key *)&key, (void *)data, dsize, flags, type);
	if (err) {
		std::ostringstream str;
		str << "eblob write failed: dsize: " << dsize << ": " << strerror(-err);
		throw std::runtime_error(str.str());
	}
}

void eblob::write(const struct eblob_key &key, const std::string &data, uint64_t flags, int type)
{
	write(key, data.data(), data.size(), flags, type);
}

int eblob::read(const struct eblob_key &key, int *fd, uint64_t *offset, uint64_t *size, int type)
{
	int err;

	err = eblob_read(eblob_, (struct eblob_key *)&key, fd, offset, size, type);
	if (err < 0) {
		std::ostringstream str;
		str << "eblob read failed: " << strerror(-err);
		throw std::runtime_error(str.str());
	}

	return err;
}

std::string eblob::read(const struct eblob_key &key, const uint64_t req_offset, const uint64_t req_size, int type)
{
	std::string ret;

	int err;
	char *data;
	uint64_t dsize = req_size;

	err = eblob_read_data(eblob_, (struct eblob_key *)&key, req_offset, &data, &dsize, type);
	if (err < 0) {
		std::ostringstream str;
		str << "eblob read failed: " << strerror(-err);
		throw std::runtime_error(str.str());
	}

	try {
		ret.assign(data, dsize);
	} catch (...) {
		free(data);
		throw;
	}
	free(data);

	return ret;
}

void eblob::write_hashed(const std::string &key, const std::string &data, uint64_t flags, int type)
{
	struct eblob_key ekey;

	eblob_hash(eblob_, ekey.id, sizeof(ekey.id), key.data(), key.size());
	write(ekey, data, flags, type);
}

void eblob::read_hashed(const std::string &key, int *fd, uint64_t *offset, uint64_t *size, int type)
{
	struct eblob_key ekey;

	eblob_hash(eblob_, ekey.id, sizeof(ekey.id), key.data(), key.size());
	read(ekey, fd, offset, size, type);
}

std::string eblob::read_hashed(const std::string &key, const uint64_t offset, const uint64_t size, int type)
{
	struct eblob_key ekey;

	eblob_hash(eblob_, ekey.id, sizeof(ekey.id), key.data(), key.size());
	return read(ekey, offset, size, type);
}

void eblob::remove(const struct eblob_key &key, int type)
{
	eblob_remove(eblob_, (struct eblob_key *)&key, type);
}

void eblob::remove_all(const struct eblob_key &key)
{
	eblob_remove_all(eblob_, (struct eblob_key *)&key);
}

unsigned long long eblob::elements(void)
{
	return eblob_total_elements(eblob_);
}

void eblob::remove_hashed(const std::string &key, int type)
{
	eblob_remove_hashed(eblob_, key.data(), key.size(), type);
}
