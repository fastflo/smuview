/*
 * This file is part of the SmuView project.
 *
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

#include <libsigrokcxx/libsigrokcxx.hpp>

#include "timecurve.hpp"
#include "src/util.hpp"
#include "src/data/analogsignal.hpp"

namespace sv {
namespace data {

TimeCurve::TimeCurve(shared_ptr<AnalogSignal> signal) :
	BaseCurve(),
	analog_signal_(signal)
{
	signal_start_timestamp_ = signal->signal_start_timestamp();
}

QPointF TimeCurve::sample(size_t i) const
{
	//signal_data_->lock();

	sample_t sample = analog_signal_->get_sample(i);
	QPointF sample_point(sample.first - signal_start_timestamp_, sample.second);

	//signal_data_->.unlock();

	return sample_point;
}

size_t TimeCurve::size() const
{
	// TODO: Synchronize x/y sample data
	return analog_signal_->get_sample_count();
}

QRectF TimeCurve::boundingRect() const
{
	// top left, bottom right
	return QRectF(
		QPointF(analog_signal_->first_timestamp() - signal_start_timestamp_,
				analog_signal_->max_value()),
		QPointF(analog_signal_->last_timestamp() - signal_start_timestamp_,
				analog_signal_->min_value()));
}

QString TimeCurve::x_data_quantity() const
{
	return util::format_sr_quantity(sigrok::Quantity::TIME);
}

QString TimeCurve::x_data_unit() const
{
	return util::format_sr_unit(sigrok::Unit::SECOND);
}

QString TimeCurve::x_data_title() const
{
	return QString("%1 [%2]").arg(x_data_quantity()).arg(x_data_unit());
}

QString TimeCurve::y_data_quantity() const
{
	return analog_signal_->quantity();
}

QString TimeCurve::y_data_unit() const
{
	return analog_signal_->unit();
}

QString TimeCurve::y_data_title() const
{
	return QString("%1 [%2]").arg(y_data_quantity()).arg(y_data_unit());
}

} // namespace data
} // namespace sv
