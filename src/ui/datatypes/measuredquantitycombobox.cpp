/*
 * This file is part of the SmuView project.
 *
 * Copyright (C) 2018-2020 Frank Stettner <frank-stettner@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <set>
#include <stdexcept>
#include <vector>

#include <QDebug>
#include <QString>
#include <QVariant>

#include "measuredquantitycombobox.hpp"
#include "src/util.hpp"
#include "src/data/datautil.hpp"
#include "src/data/properties/baseproperty.hpp"
#include "src/data/properties/measuredquantityproperty.hpp"
#include "src/devices/configurable.hpp"

using std::dynamic_pointer_cast;
using std::set;

Q_DECLARE_METATYPE(sv::data::measured_quantity_t)

namespace sv {
namespace ui {
namespace datatypes {

MeasuredQuantityComboBox::MeasuredQuantityComboBox(
		shared_ptr<sv::data::properties::BaseProperty> property,
		const bool auto_commit, const bool auto_update,
		QWidget *parent) :
	QComboBox(parent),
	BaseWidget(property, auto_commit, auto_update)
{
	// Check property
	if (property_ != nullptr &&
			property_->data_type() != data::DataType::MQ) {

		QString msg =
			QString("MeasuredQuantityComboBox with property of type ").append(
				data::datautil::format_data_type(property_->data_type()));
		throw std::runtime_error(msg.toStdString());
	}

	setup_ui();
	connect_signals();
}

void MeasuredQuantityComboBox::setup_ui()
{
	//this->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);
	if (property_ != nullptr && property_->is_listable()) {
		shared_ptr<data::properties::MeasuredQuantityProperty> mq_prop =
			dynamic_pointer_cast<data::properties::MeasuredQuantityProperty>(
				property_);

		for (const auto &mq : mq_prop->list_values()) {
			this->addItem(
				data::datautil::format_measured_quantity(mq),
				QVariant::fromValue(mq));
		}
	}
	else if (property_ != nullptr && property_->is_getable()) {
		this->addItem(property_->to_string(), property_->value());
	}
	if (property_ == nullptr || !property_->is_setable())
		this->setDisabled(true);
	if (property_ != nullptr && property_->is_getable())
		on_value_changed(property_->value());
}

void MeasuredQuantityComboBox::connect_signals()
{
	// Widget -> Property
	connect_widget_2_prop_signals();

	// Property -> Widget
	if (auto_update_ && property_ != nullptr) {
		connect(property_.get(), SIGNAL(value_changed(const QVariant)),
			this, SLOT(on_value_changed(const QVariant)));
		connect(property_.get(), &data::properties::BaseProperty::list_changed,
			this, &MeasuredQuantityComboBox::on_list_changed);
	}
}

void MeasuredQuantityComboBox::connect_widget_2_prop_signals()
{
	if (auto_commit_ && property_ != nullptr && property_->is_setable()) {
		connect(this, SIGNAL(currentIndexChanged(int)),
			this, SLOT(value_changed(int)));
	}
}

void MeasuredQuantityComboBox::disconnect_widget_2_prop_signals()
{
	if (auto_commit_ && property_ != nullptr && property_->is_setable()) {
		disconnect(this, SIGNAL(currentIndexChanged(int)),
			this, SLOT(value_changed(int)));
	}
}

QVariant MeasuredQuantityComboBox::variant_value() const
{
	return QVariant(this->currentData());
}

void MeasuredQuantityComboBox::value_changed(int index)
{
	(void)index;

	if (property_ != nullptr) {
		data::measured_quantity_t value =
			this->currentData().value<data::measured_quantity_t>();
		property_->change_value(QVariant().fromValue(value));
	}
}

void MeasuredQuantityComboBox::on_value_changed(const QVariant qvar)
{
	// Disconnect Widget -> Property signal to prevent echoing
	disconnect_widget_2_prop_signals();

	this->setCurrentText(data::datautil::format_measured_quantity(
		qvar.value<data::measured_quantity_t>()));

	connect_widget_2_prop_signals();
}

void MeasuredQuantityComboBox::on_list_changed()
{
	// Disconnect Widget -> Property signal to prevent echoing
	disconnect_widget_2_prop_signals();

	if (property_ != nullptr && property_->is_listable()) {
		this->clear();

		shared_ptr<data::properties::MeasuredQuantityProperty> mq_prop =
			dynamic_pointer_cast<data::properties::MeasuredQuantityProperty>(
				property_);
		for (const auto &mq : mq_prop->list_values()) {
			this->addItem(
				data::datautil::format_measured_quantity(mq),
				QVariant::fromValue(mq));
		}

		if (property_->is_getable())
			this->setCurrentText(data::datautil::format_measured_quantity(
				mq_prop->measured_quantity_value()));
	}

	connect_widget_2_prop_signals();
}

} // namespace datatypes
} // namespace ui
} // namespece sv
