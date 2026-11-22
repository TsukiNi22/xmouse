/**************************************************************\

 ██╗  ██╗ █████╗ ██████╗ ████████╗ █████╗ ███╗   ██╗██╗ █████╗
 ╚██╗██╔╝██╔══██╗██╔══██╗╚══██╔══╝██╔══██╗████╗  ██║██║██╔══██╗
  ╚███╔╝ ███████║██████╔╝   ██║   ███████║██╔██╗ ██║██║███████║
  ██╔██╗ ██╔══██║██╔══██╗   ██║   ██╔══██║██║╚██╗██║██║██╔══██║
 ██╔╝ ██╗██║  ██║██║  ██║   ██║   ██║  ██║██║ ╚████║██║██║  ██║
 ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝╚═╝  ╚═╝

Edition:
##  @date 22/11/2026 by @author Tsukini

File Name:
##  @file State.cpp

File Description:
##  You know, I don t think there are good or bad descriptions,
##  for me, life is all about functions...
\**************************************************************/

#define _Attribute
#define _Exception
#include <utils/utils.hpp>
#include "xmouse/core/State.hpp"
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <QFile>

// Entry <-> {"v", "c", "a", "d"}
_hot static QJsonObject entry_to_json(const xmouse::core::StateEntry& entry)
{
    QJsonObject o{{"c", QString::number(entry.clock)}, {"a", entry.author}};

    if (entry.deleted) o["d"] = true;
    else o["v"] = entry.value;
    return o;
}

_hot static xmouse::core::StateEntry entry_from_json(const QJsonObject& o)
{
    xmouse::core::StateEntry entry;

    entry.clock = o.value("c").toString().toULongLong();
    entry.author = o.value("a").toString().left(64);
    entry.deleted = o.value("d").toBool(false);
    entry.value = entry.deleted ? QJsonValue(QJsonValue::Null) : o.value("v");
    return entry;
}

bool xmouse::core::State::newer_(const StateEntry& a, const StateEntry& b)
{
    if (a.clock != b.clock) return a.clock > b.clock;
    if (a.author != b.author) return a.author > b.author;
    return a.deleted && !b.deleted; // same write seen twice: a removal wins (never resurrected)
}

QJsonObject xmouse::core::State::set(const QString& key, const QJsonValue& value)
{
    StateEntry entry;
    auto it = this->_entries.find(key);

    if (it != this->_entries.end() && !it->deleted && it->value == value) return {};
    entry.value = value;
    entry.clock = ++this->_clock;
    entry.author = this->_self;
    this->_entries.insert(key, entry);
    return QJsonObject{{key, entry_to_json(entry)}};
}

QJsonObject xmouse::core::State::remove(const QString& key)
{
    StateEntry entry;
    auto it = this->_entries.find(key);

    if (it == this->_entries.end() || it->deleted) return {};
    entry.clock = ++this->_clock;
    entry.author = this->_self;
    entry.deleted = true;
    this->_entries.insert(key, entry);
    return QJsonObject{{key, entry_to_json(entry)}};
}

QStringList xmouse::core::State::merge(const QJsonObject& delta)
{
    QStringList changed;

    for (auto it = delta.begin(); it != delta.end(); ++it) {
        const StateEntry entry = entry_from_json(it.value().toObject());
        auto current = this->_entries.find(it.key());

        if (it.key().isEmpty() || it.key().size() > 256) continue;
        this->_clock = std::max(this->_clock, entry.clock);
        if (current != this->_entries.end() && !newer_(entry, *current)) continue;

        // Only a change of the visible value is told (a newer write of the same value is just stored)
        const bool visible = current == this->_entries.end() ? !entry.deleted : (current->deleted != entry.deleted || current->value != entry.value);
        this->_entries.insert(it.key(), entry);
        if (visible) changed.append(it.key());
    }
    return changed;
}

QJsonValue xmouse::core::State::value(const QString& key) const
{
    auto it = this->_entries.constFind(key);

    if (it == this->_entries.constEnd() || it->deleted) return QJsonValue::Null;
    return it->value;
}

QStringList xmouse::core::State::keys(const QString& prefix) const
{
    QStringList out;

    for (auto it = this->_entries.constBegin(); it != this->_entries.constEnd(); ++it)
        if (!it->deleted && it.key().startsWith(prefix)) out.append(it.key());
    out.sort();
    return out;
}

QStringList xmouse::core::State::ids(const QString& prefix) const
{
    QStringList out;

    for (const QString& key: this->keys(prefix))
        out.append(key.mid(prefix.size()));
    return out;
}

QJsonObject xmouse::core::State::toJson(void) const
{
    QJsonObject o;

    for (auto it = this->_entries.constBegin(); it != this->_entries.constEnd(); ++it)
        o[it.key()] = entry_to_json(it.value());
    return o;
}

void xmouse::core::State::load(const QString& path)
{
    QFile file(path);
    QJsonParseError error;
    QJsonObject o;

    this->clear();
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot read " + path.toStdString());
    o = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError) return; // a broken copy: the members send it again
    (void)this->merge(o);
}

void xmouse::core::State::save(const QString& path) const
{
    QSaveFile file(path);

    if (!file.open(QIODevice::WriteOnly)) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot write " + path.toStdString());
    file.write(QJsonDocument(this->toJson()).toJson(QJsonDocument::Compact));
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!file.commit()) _unlikely
        throw utils::exception::ErrorException(utils::exception::ExternalCode::Io, "Cannot write " + path.toStdString());
}
