/*
 * This file is part of the SmuView project.
 *
 * Copyright (C) 2015 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2017 Frank Stettner <frank-stettner@gmx.net>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <cassert>
#include <glib.h>
#include <thread>
#include <boost/algorithm/string/join.hpp>

#include <QDateTime>
#include <QDebug>
#include <QString>

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "hardwaredevice.hpp"
#include "src/devicemanager.hpp"
#include "src/session.hpp"
#include "src/devices/configurable.hpp"
#include "src/devices/device.hpp"
#include "src/data/analogsignal.hpp"
#include "src/data/basesignal.hpp"

using std::bad_alloc;
using std::dynamic_pointer_cast;
using std::lock_guard;
using std::make_shared;
using std::map;
using std::pair;
using std::set;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;
using std::unique_ptr;

using boost::algorithm::join;

namespace sv {
namespace devices {

HardwareDevice::HardwareDevice(
		const shared_ptr<sigrok::Context> &sr_context,
		shared_ptr<sigrok::HardwareDevice> sr_device) :
	Device(sr_context, sr_device)
{
	// Sigrok Channel Groups
	map<string, shared_ptr<sigrok::ChannelGroup>> sr_channel_groups =
		sr_device_->channel_groups();
	if (sr_channel_groups.size() > 0) {
		for (auto sr_cg_pair : sr_channel_groups) {
			shared_ptr<sigrok::ChannelGroup> sr_cg = sr_cg_pair.second;
			configurables_.push_back(make_shared<devices::Configurable>(sr_cg));

			vector<shared_ptr<data::BaseSignal>> cg_signals;
			for (auto sr_c : sr_cg->channels()) {
				if (sr_channel_signal_map_.count(sr_c) > 0)
					cg_signals.push_back(sr_channel_signal_map_[sr_c]);
			}
			channel_group_name_signals_map_.insert(
				pair<QString, vector<shared_ptr<data::BaseSignal>>>
					(QString::fromStdString(sr_cg->name()), cg_signals));
		}
	}
	else {
		configurables_.push_back(make_shared<Configurable>(sr_device_));
	}
}

HardwareDevice::~HardwareDevice()
{
	close();
}

QString HardwareDevice::full_name() const
{
	QString sep("");
	QString name("");

	if (sr_device_->vendor().length() > 0) {
		name.append(QString::fromStdString(sr_device_->vendor()));
		name.append(sep);
		sep = QString(" ");
	}

	if (sr_device_->model().length() > 0) {
		name.append(QString::fromStdString(sr_device_->model()));
		name.append(sep);
		sep = QString(" ");
	}

	if (sr_device_->version().length() > 0) {
		name.append(QString::fromStdString(sr_device_->version()));
		name.append(sep);
		sep = QString(" ");
	}

	if (sr_device_->serial_number().length() > 0) {
		name.append(QString::fromStdString(sr_device_->serial_number()));
		name.append(sep);
		sep = QString(" ");
	}

	if (sr_device_->connection_id().length() > 0) {
		name.append(sep);
		name.append("(");
		name.append(QString::fromStdString(sr_device_->connection_id()));
		name.append(")");
	}

	return name;
}

QString HardwareDevice::short_name() const
{
	QString sep("");
	QString name("");

	if (sr_device_->vendor().length() > 0) {
		name.append(QString::fromStdString(sr_device_->vendor()));
		name.append(sep);
		sep = QString(" ");
	}

	if (sr_device_->model().length() > 0) {
		name.append(QString::fromStdString(sr_device_->model()));
		name.append(sep);
		sep = QString(" ");
	}

	if (sr_device_->connection_id().length() > 0) {
		name.append(sep);
		name.append("(");
		name.append(QString::fromStdString(sr_device_->connection_id()));
		name.append(")");
	}

	return name;
}

shared_ptr<sigrok::HardwareDevice> HardwareDevice::sr_hardware_device() const
{
	return static_pointer_cast<sigrok::HardwareDevice>(sr_device_);
}

string HardwareDevice::display_name(
	const DeviceManager &device_manager) const
{
	const auto hw_dev = sr_hardware_device();

	// If we can find another device with the same model/vendor then
	// we have at least two such devices and need to distinguish them.
	const auto &devices = device_manager.devices();
	const bool multiple_dev = hw_dev && any_of(
		devices.begin(), devices.end(),
		[&](shared_ptr<devices::HardwareDevice> dev) {
			return dev->sr_hardware_device()->vendor() ==
					hw_dev->vendor() &&
				dev->sr_hardware_device()->model() ==
					hw_dev->model() &&
				dev->sr_device_ != sr_device_;
		});

	vector<string> parts = {
		sr_device_->vendor(), sr_device_->model() };

	if (multiple_dev) {
		parts.push_back(sr_device_->version());
		parts.push_back(sr_device_->serial_number());

		if ((sr_device_->serial_number().length() == 0) &&
			(sr_device_->connection_id().length() > 0))
			parts.push_back("(" + sr_device_->connection_id() + ")");
	}

	return join(parts, " ");
}

vector<shared_ptr<data::AnalogSignal>> HardwareDevice::all_signals() const
{
	return all_signals_;
}

map<QString, vector<shared_ptr<data::BaseSignal>>>
	HardwareDevice::channel_group_name_signals_map() const
{
	return channel_group_name_signals_map_;
}

vector<shared_ptr<devices::Configurable>> HardwareDevice::configurables() const
{
	return configurables_;
}

void HardwareDevice::feed_in_header()
{
}

void HardwareDevice::feed_in_trigger()
{
}

void HardwareDevice::feed_in_frame_begin()
{
	frame_began_ = true;
}

void HardwareDevice::feed_in_frame_end()
{
	if (frame_began_ && actual_processed_signal_) {
		/*
		qWarning() << "feed_in_frame_end(): Set timestamp to " <<
			actual_processed_signal_->name();
		*/
		// TODO: This may not work reliably
		//actual_processed_signal_->add_timestamp();
		//actual_processed_signal_ = nullptr;

	}
	frame_began_ = false;
}

void HardwareDevice::feed_in_logic(shared_ptr<sigrok::Logic> sr_logic)
{
	(void)sr_logic;
}

void HardwareDevice::feed_in_analog(shared_ptr<sigrok::Analog> sr_analog)
{
	lock_guard<recursive_mutex> lock(data_mutex_);

	const vector<shared_ptr<sigrok::Channel>> sr_channels = sr_analog->channels();
	//const unsigned int channel_count = sr_channels.size();
	//const size_t sample_count = sr_analog->num_samples() / channel_count;

	unique_ptr<float> data(new float[sr_analog->num_samples()]);
	sr_analog->get_data_as_float(data.get());

	float *channel_data = data.get();
	for (auto sr_channel : sr_channels) {
		/*
		qWarning() << "HardwareDevice::feed_in_analog(): HardwareDevice = " <<
			QString::fromStdString(sr_device_->model()) <<
			", Channel.Id = " <<
			QString::fromStdString(sr_channel->name()) <<
			" channel_data = " << *channel_data;
		*/

		if (!sr_channel_signal_map_.count(sr_channel)) {
			qWarning() << "HardwareDevice::feed_in_analog(): Unknown channel '" <<
				QString::fromStdString(sr_channel->name()) << "'. Abort!";
			assert("Unknown channel");
		}

		actual_processed_signal_ = sr_channel_signal_map_[sr_channel];
		/*
		qWarning() << "HardwareDevice::feed_in_analog(): -3- name = " << actual_processed_signal_->name();
		qWarning() << "HardwareDevice::feed_in_analog(): -3- count = " << actual_processed_signal_->analog_data()->get_sample_count();
		*/

		/* TODO: Use push_interleaved_samples() as only push function
		actual_processed_signal_->analog_data()->push_interleaved_samples(
			channel_data, sample_count,channel_count, sr_analog->unit());
		*/
		actual_processed_signal_->push_sample(channel_data, sr_analog->unit());
		channel_data++;

		// Timestamp for values not in a FRAME
		//if (!frame_began_)
		//	actual_processed_signal_->add_timestamp();

		/*
		Q_EMIT data_received(segment);
		*/
	}

	/*
	qWarning() << "HardwareDevice::feed_in_analog(): -END-";
	*/
}

} // namespace devices
} // namespace sv
