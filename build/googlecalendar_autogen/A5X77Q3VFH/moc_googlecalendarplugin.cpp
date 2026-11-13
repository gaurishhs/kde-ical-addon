/****************************************************************************
** Meta object code from reading C++ file 'googlecalendarplugin.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/plugin/googlecalendarplugin.h"
#include <QtCore/qmetatype.h>
#include <QtCore/qplugin.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'googlecalendarplugin.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN20GoogleCalendarPluginE_t {};
} // unnamed namespace

template <> constexpr inline auto GoogleCalendarPlugin::qt_create_metaobjectdata<qt_meta_tag_ZN20GoogleCalendarPluginE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "GoogleCalendarPlugin"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<GoogleCalendarPlugin, qt_meta_tag_ZN20GoogleCalendarPluginE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject GoogleCalendarPlugin::staticMetaObject = { {
    QMetaObject::SuperData::link<CalendarEvents::CalendarEventsPlugin::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20GoogleCalendarPluginE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20GoogleCalendarPluginE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN20GoogleCalendarPluginE_t>.metaTypes,
    nullptr
} };

void GoogleCalendarPlugin::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GoogleCalendarPlugin *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *GoogleCalendarPlugin::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *GoogleCalendarPlugin::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20GoogleCalendarPluginE_t>.strings))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "org.kde.CalendarEventsPlugin"))
        return static_cast< CalendarEvents::CalendarEventsPlugin*>(this);
    return CalendarEvents::CalendarEventsPlugin::qt_metacast(_clname);
}

int GoogleCalendarPlugin::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = CalendarEvents::CalendarEventsPlugin::qt_metacall(_c, _id, _a);
    return _id;
}

#ifdef QT_MOC_EXPORT_PLUGIN_V2
static constexpr unsigned char qt_pluginMetaDataV2_GoogleCalendarPlugin[] = {
    0xbf, 
    // "IID"
    0x02,  0x78,  0x1c,  'o',  'r',  'g',  '.',  'k', 
    'd',  'e',  '.',  'C',  'a',  'l',  'e',  'n', 
    'd',  'a',  'r',  'E',  'v',  'e',  'n',  't', 
    's',  'P',  'l',  'u',  'g',  'i',  'n', 
    // "className"
    0x03,  0x74,  'G',  'o',  'o',  'g',  'l',  'e', 
    'C',  'a',  'l',  'e',  'n',  'd',  'a',  'r', 
    'P',  'l',  'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xa2,  0x67,  'K',  'P',  'l',  'u',  'g', 
    'i',  'n',  0xa6,  0x67,  'A',  'u',  't',  'h', 
    'o',  'r',  's',  0x81,  0xa2,  0x65,  'E',  'm', 
    'a',  'i',  'l',  0x6f,  'y',  'o',  'u',  '@', 
    'e',  'x',  'a',  'm',  'p',  'l',  'e',  '.', 
    'o',  'r',  'g',  0x64,  'N',  'a',  'm',  'e', 
    0x78,  0x25,  'G',  'o',  'o',  'g',  'l',  'e', 
    ' ',  'C',  'a',  'l',  'e',  'n',  'd',  'a', 
    'r',  ' ',  'P',  'l',  'a',  's',  'm',  'a', 
    ' ',  'p',  'l',  'u',  'g',  'i',  'n',  ' ', 
    'a',  'u',  't',  'h',  'o',  'r',  's',  0x6b, 
    'D',  'e',  's',  'c',  'r',  'i',  'p',  't', 
    'i',  'o',  'n',  0x78,  0x33,  'S',  'h',  'o', 
    'w',  ' ',  'e',  'v',  'e',  'n',  't',  's', 
    ' ',  'f',  'r',  'o',  'm',  ' ',  'p',  'r', 
    'i',  'v',  'a',  't',  'e',  ' ',  'G',  'o', 
    'o',  'g',  'l',  'e',  ' ',  'C',  'a',  'l', 
    'e',  'n',  'd',  'a',  'r',  ' ',  'i',  'C', 
    'a',  'l',  ' ',  'f',  'e',  'e',  'd',  's', 
    0x64,  'I',  'c',  'o',  'n',  0x6d,  'v',  'i', 
    'e',  'w',  '-',  'c',  'a',  'l',  'e',  'n', 
    'd',  'a',  'r',  0x67,  'L',  'i',  'c',  'e', 
    'n',  's',  'e',  0x70,  'G',  'P',  'L',  '-', 
    '2',  '.',  '0',  '-',  'o',  'r',  '-',  'l', 
    'a',  't',  'e',  'r',  0x64,  'N',  'a',  'm', 
    'e',  0x76,  'G',  'o',  'o',  'g',  'l',  'e', 
    ' ',  'C',  'a',  'l',  'e',  'n',  'd',  'a', 
    'r',  ' ',  '(',  'i',  'C',  'a',  'l',  ')', 
    0x67,  'V',  'e',  'r',  's',  'i',  'o',  'n', 
    0x65,  '2',  '.',  '0',  '.',  '0',  0x78,  0x1d, 
    'X',  '-',  'K',  'D',  'E',  '-',  'P',  'l', 
    'a',  's',  'm',  'a',  'C',  'a',  'l',  'e', 
    'n',  'd',  'a',  'r',  '-',  'C',  'o',  'n', 
    'f',  'i',  'g',  'U',  'i',  0x78,  0x27,  'g', 
    'o',  'o',  'g',  'l',  'e',  'c',  'a',  'l', 
    'e',  'n',  'd',  'a',  'r',  '/',  'G',  'o', 
    'o',  'g',  'l',  'e',  'C',  'a',  'l',  'e', 
    'n',  'd',  'a',  'r',  'C',  'o',  'n',  'f', 
    'i',  'g',  '.',  'q',  'm',  'l', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN_V2(GoogleCalendarPlugin, GoogleCalendarPlugin, qt_pluginMetaDataV2_GoogleCalendarPlugin)
#else
QT_PLUGIN_METADATA_SECTION
Q_CONSTINIT static constexpr unsigned char qt_pluginMetaData_GoogleCalendarPlugin[] = {
    'Q', 'T', 'M', 'E', 'T', 'A', 'D', 'A', 'T', 'A', ' ', '!',
    // metadata version, Qt version, architectural requirements
    0, QT_VERSION_MAJOR, QT_VERSION_MINOR, qPluginArchRequirements(),
    0xbf, 
    // "IID"
    0x02,  0x78,  0x1c,  'o',  'r',  'g',  '.',  'k', 
    'd',  'e',  '.',  'C',  'a',  'l',  'e',  'n', 
    'd',  'a',  'r',  'E',  'v',  'e',  'n',  't', 
    's',  'P',  'l',  'u',  'g',  'i',  'n', 
    // "className"
    0x03,  0x74,  'G',  'o',  'o',  'g',  'l',  'e', 
    'C',  'a',  'l',  'e',  'n',  'd',  'a',  'r', 
    'P',  'l',  'u',  'g',  'i',  'n', 
    // "MetaData"
    0x04,  0xa2,  0x67,  'K',  'P',  'l',  'u',  'g', 
    'i',  'n',  0xa6,  0x67,  'A',  'u',  't',  'h', 
    'o',  'r',  's',  0x81,  0xa2,  0x65,  'E',  'm', 
    'a',  'i',  'l',  0x6f,  'y',  'o',  'u',  '@', 
    'e',  'x',  'a',  'm',  'p',  'l',  'e',  '.', 
    'o',  'r',  'g',  0x64,  'N',  'a',  'm',  'e', 
    0x78,  0x25,  'G',  'o',  'o',  'g',  'l',  'e', 
    ' ',  'C',  'a',  'l',  'e',  'n',  'd',  'a', 
    'r',  ' ',  'P',  'l',  'a',  's',  'm',  'a', 
    ' ',  'p',  'l',  'u',  'g',  'i',  'n',  ' ', 
    'a',  'u',  't',  'h',  'o',  'r',  's',  0x6b, 
    'D',  'e',  's',  'c',  'r',  'i',  'p',  't', 
    'i',  'o',  'n',  0x78,  0x33,  'S',  'h',  'o', 
    'w',  ' ',  'e',  'v',  'e',  'n',  't',  's', 
    ' ',  'f',  'r',  'o',  'm',  ' ',  'p',  'r', 
    'i',  'v',  'a',  't',  'e',  ' ',  'G',  'o', 
    'o',  'g',  'l',  'e',  ' ',  'C',  'a',  'l', 
    'e',  'n',  'd',  'a',  'r',  ' ',  'i',  'C', 
    'a',  'l',  ' ',  'f',  'e',  'e',  'd',  's', 
    0x64,  'I',  'c',  'o',  'n',  0x6d,  'v',  'i', 
    'e',  'w',  '-',  'c',  'a',  'l',  'e',  'n', 
    'd',  'a',  'r',  0x67,  'L',  'i',  'c',  'e', 
    'n',  's',  'e',  0x70,  'G',  'P',  'L',  '-', 
    '2',  '.',  '0',  '-',  'o',  'r',  '-',  'l', 
    'a',  't',  'e',  'r',  0x64,  'N',  'a',  'm', 
    'e',  0x76,  'G',  'o',  'o',  'g',  'l',  'e', 
    ' ',  'C',  'a',  'l',  'e',  'n',  'd',  'a', 
    'r',  ' ',  '(',  'i',  'C',  'a',  'l',  ')', 
    0x67,  'V',  'e',  'r',  's',  'i',  'o',  'n', 
    0x65,  '2',  '.',  '0',  '.',  '0',  0x78,  0x1d, 
    'X',  '-',  'K',  'D',  'E',  '-',  'P',  'l', 
    'a',  's',  'm',  'a',  'C',  'a',  'l',  'e', 
    'n',  'd',  'a',  'r',  '-',  'C',  'o',  'n', 
    'f',  'i',  'g',  'U',  'i',  0x78,  0x27,  'g', 
    'o',  'o',  'g',  'l',  'e',  'c',  'a',  'l', 
    'e',  'n',  'd',  'a',  'r',  '/',  'G',  'o', 
    'o',  'g',  'l',  'e',  'C',  'a',  'l',  'e', 
    'n',  'd',  'a',  'r',  'C',  'o',  'n',  'f', 
    'i',  'g',  '.',  'q',  'm',  'l', 
    0xff, 
};
QT_MOC_EXPORT_PLUGIN(GoogleCalendarPlugin, GoogleCalendarPlugin)
#endif  // QT_MOC_EXPORT_PLUGIN_V2

QT_WARNING_POP
