#include <ei.h>
#include <QThread>
#include <QDateTime>
#include <QApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlEngine>

class eqmlTerm
{
	const char * _buf;
	int _index;
	int _type;
	int _arity;
	int _size;
public:
	eqmlTerm(const eqmlTerm &);
	eqmlTerm(const char * buf, int index = 0) : _buf(buf), _index(index)
	{
		if (index == 0)
			ei_decode_version(_buf, &_index, NULL);

		ei_get_type(_buf, &_index, &_type, &_size);
		
		if (isTuple())
			ei_decode_tuple_header(_buf, &_index, &_arity);
	}

	eqmlTerm operator[] (int elem) const
	{
		int idx = _index;
		for (int i = 1; i < elem; ++i)
			ei_skip_term(_buf, &idx);

		return eqmlTerm(_buf, idx);
	}

	QByteArray atom() const
	{
		int idx = _index; char p[MAXATOMLEN];
		ei_decode_atom(_buf, &idx, p);
		return QByteArray(p);
	}

	bool isTuple() const
	{ 
		return _type == ERL_SMALL_TUPLE_EXT || _type == ERL_LARGE_TUPLE_EXT;
	}

	bool isDouble() const
	{ 
		return _type == ERL_FLOAT_EXT || _type == NEW_FLOAT_EXT;
	}

	bool isAtom() const
	{ 
		return 
			_type == ERL_ATOM_EXT ||
			_type == ERL_SMALL_ATOM_EXT ||
			_type == ERL_ATOM_UTF8_EXT ||
			_type == ERL_SMALL_ATOM_UTF8_EXT;
	}

	bool isInteger() const
	{ 
		return 
			_type == ERL_INTEGER_EXT ||
			_type == ERL_SMALL_INTEGER_EXT ||
			_type == ERL_SMALL_BIG_EXT ||
			_type == ERL_LARGE_BIG_EXT;
	}

	bool isString() const
	{ 
		return 
			_type == ERL_LIST_EXT ||
			_type == ERL_STRING_EXT ||
			_type == ERL_NIL_EXT;
	}

	bool toBool() const
	{
		QByteArray a = atom();
		if (a == "true" ) return true;
		if (a == "false") return false;

		qWarning("can't cast term to bool");
		return false;
	}

	int toInteger() const
	{ 
		int idx = _index; long p;
		ei_decode_long(_buf, &idx, &p);
		return p;
	}

	double toDouble() const
	{ 
		int idx = _index; double p;
		ei_decode_double(_buf, &idx, &p);
		return p;
	}

	double toScalar() const
	{
		if (isInteger())
			return toInteger();
		if (isDouble())
			return toDouble();

		qWarning("can't cast term to scalar");
		return 0.0;
	}

	QString toString() const
	{
		int idx = _index; QByteArray a(_size, 0);
		ei_decode_string(_buf, &idx, a.data());
		return QString(a);
	}

	QByteArray toArray() const
	{
		int idx = _index; QByteArray a(_size, 0);
		if (isAtom())
			ei_decode_atom(_buf, &idx, a.data());
		else
			ei_decode_string(_buf, &idx, a.data());

		return a;
	}
};

class eqmlLink : public QObject
{
	Q_OBJECT

	char _buf[1024];
	int _index;
	QDataStream & _os;

	void end(int index)
	{
		_os << index;
		_os.writeRawData(_buf, index);
	}

	int push(int index, const QVariant & v)
	{
		switch (v.type())
		{
			case QMetaType::Int:
				ei_encode_long(_buf, &index, v.toInt());
				break;
			case QMetaType::Double:
				ei_encode_double(_buf, &index, v.toDouble());
				break;
			case QMetaType::QString:
				ei_encode_string(_buf, &index, qPrintable(v.toString()));
				break;
			default:
				qWarning("can't cast QVariant to term");
		}
		return index;
	}

public:
	eqmlLink(QDataStream & os, QObject * obj, const eqmlTerm & term)
		: _index(0)
		, _os(os)
	{
		int order = term[4].toInteger();

		QByteArray Args = "(" + QByteArray("QVariant").repeated(order) + ")";
		Args.replace("tQ", "t,Q");

		int signalIdx = obj->metaObject()->indexOfSignal(term[2].toArray() + Args);
		QMetaMethod signalMethod = obj->metaObject()->method(signalIdx);

		int slotIdx = metaObject()->indexOfSlot("link" + Args);
		QMetaMethod slotMethod = metaObject()->method(slotIdx);

		if (!QObject::connect(obj, signalMethod, this, slotMethod))
			qWarning("connection fail");

		ei_encode_version(_buf, &_index);
		ei_encode_tuple_header(_buf, &_index, order + 3);
		ei_encode_atom(_buf, &_index, "signal");
		ei_encode_string(_buf, &_index, term[5].toArray().data());
		ei_encode_atom(_buf, &_index, term[3].toArray().data());
	}

public slots:
	void link()
	{
		end(_index);
	}

	void link(const QVariant& a)
	{
		end(push(_index, a));
	}

	void link(const QVariant& a, const QVariant& b)
	{
		end(push(push(_index, a), b));
	}

	void link(const QVariant& a, const QVariant& b, const QVariant& c)
	{
		end(push(push(push(_index, a), b), c));
	}

	void link(const QVariant& a, const QVariant& b, const QVariant& c, const QVariant& d)
	{
		end(push(push(push(push(_index, a), b), c), d));
	}

	void link(const QVariant& a, const QVariant& b, const QVariant& c, const QVariant& d, const QVariant& e)
	{
		end(push(push(push(push(push(_index, a), b), c), d), e));
	}
};

class eqmlPipe : public QThread
{
	Q_OBJECT
public:
	void run()
	{
		QFile inFile;
		inFile.open(3, QIODevice::ReadOnly | QIODevice::Unbuffered);		
		QDataStream inStream(&inFile);

		QByteArray buffer;
		quint32 len;

		next:
		inStream >> len;
		if (inStream.status() == QDataStream::Ok)
		{
			buffer.resize(len);
			if (inStream.readRawData(buffer.data(), len) >= 0)
			{
				emit packet(buffer);
				goto next;
			}
		}
	}
signals:
	void packet(QByteArray a);
};

class eqmlWindow : public QQuickView
{
	Q_OBJECT

	typedef void (eqmlWindow::*Handle)(const eqmlTerm &);
	typedef QMap<QByteArray, Handle> Registry;
	Registry registry;

	QFile outFile;
	QDataStream outStream;

	typedef QVariant (eqmlWindow::*VarFun)(const eqmlTerm &);
	typedef QMap<QByteArray, VarFun> VarMap;
	VarMap _varMap;

	QObject * find(const eqmlTerm & term)
	{
		QByteArray name = term.toArray();
		QObject * root = rootObject();

		if (root->objectName() == name)
			return root;
		if (QObject * obj = root->findChild<QObject *>(name))
			return obj;

		qWarning("can't find child %s", name.data());
		return NULL;
	}

public:
	eqmlWindow(const QString & qmlFile)
	{
		connect(engine(), SIGNAL(quit()), SLOT(close()));
		setResizeMode(QQuickView::SizeRootObjectToView);

		setSource(QUrl::fromLocalFile(qmlFile));
		show();

		outFile.open(4, QIODevice::WriteOnly | QIODevice::Unbuffered);
		outStream.setDevice(&outFile);

		registry["connect"] = &eqmlWindow::onConnect;
		registry["set"] = &eqmlWindow::onSet;
		registry["invoke0"] = &eqmlWindow::onInvoke0;
		registry["invoke1"] = &eqmlWindow::onInvoke1;
		registry["invoke2"] = &eqmlWindow::onInvoke2;
		registry["invoke3"] = &eqmlWindow::onInvoke3;

		_varMap["url"] = &eqmlWindow::url;
		_varMap["point"] = &eqmlWindow::point;
		_varMap["datetime"] = &eqmlWindow::datetime;
	}

	QVariant var(const eqmlTerm & t)
	{
		if (t.isInteger())
			return t.toInteger();
		else if (t.isDouble())
			return t.toDouble();
		else if (t.isString())
			return t.toString();
		else if (t.isAtom())
			return t.toBool();
		else if (t.isTuple()) {
			QByteArray tag = t[1].toArray();

			VarMap::iterator it = _varMap.find(tag);
			if (it != _varMap.end())
				return (this->*it.value())(t[2]);
			else
				qWarning("unknown var \'%s\'", tag.data());
		}

		qWarning("can't cast term to QVariant");
		return QVariant();
	}

	QVariant url(const eqmlTerm & t)
	{
		return QUrl(t.toString());
	}

	QVariant point(const eqmlTerm & t)
	{
		return QPointF(t[1].toScalar(), t[2].toScalar());
	}

	QVariant datetime(const eqmlTerm & t)
	{
		const eqmlTerm & date = t[1];
		const eqmlTerm & time = t[2];

		return QDateTime(
			QDate(date[1].toInteger(), date[2].toInteger(), date[3].toInteger()),
			QTime(time[1].toInteger(), time[2].toInteger(), time[3].toInteger())
		);
	}

	void onConnect(const eqmlTerm & term)
	{
		QObject * obj = find(term[1]);
		if (!obj)
			return;

		new eqmlLink(outStream, obj, term);
	}	

	void onInvoke0(const eqmlTerm & t)
	{
		if (QObject * obj = find(t[1]))
			QMetaObject::invokeMethod(obj, t[2].toArray());
	}

	void onInvoke1(const eqmlTerm & t)
	{
		if (QObject * obj = find(t[1]))
			QMetaObject::invokeMethod(obj, t[2].toArray(),
				Q_ARG(QVariant, var(t[3])));
	}

	void onInvoke2(const eqmlTerm & t)
	{
		if (QObject * obj = find(t[1]))
			QMetaObject::invokeMethod(obj, t[2].toArray(),
				Q_ARG(QVariant, var(t[3])), Q_ARG(QVariant, var(t[4])));
	}

	void onInvoke3(const eqmlTerm & t)
	{
		if (QObject * obj = find(t[1]))
			QMetaObject::invokeMethod(obj, t[2].toArray(),
				Q_ARG(QVariant, var(t[3])), Q_ARG(QVariant, var(t[4])), Q_ARG(QVariant, var(t[5])));
	}

	void onSet(const eqmlTerm & t)
	{
		if (QObject * obj = find(t[1]))
			obj->setProperty(t[2].toArray(), var(t[3]));
	}

public slots:
	void dispatch(QByteArray buffer)
	{
		eqmlTerm term(buffer.data());
		QByteArray tag = term[1].toArray();

		Registry::iterator it = registry.find(tag);
		if (it != registry.end())
			(this->*it.value())(term[2]);
		else
			qWarning("unknown tag \'%s\', size=%d", tag.data(), tag.size());
	}
};

void eqmlLog(QtMsgType, const QMessageLogContext &, const QString & msg)
{
	fprintf(stderr, "%s\r\n", qPrintable(msg));
}

int main(int argc, char *argv[])
{
	qInstallMessageHandler(eqmlLog);
	QApplication app(argc, argv);

	if (app.arguments().size() < 2) {
		qWarning("No QML file specified");
		return -1;
	}

	eqmlPipe pipe;
	eqmlWindow win(app.arguments().at(1));

	app.connect(&pipe, SIGNAL(finished()), SLOT(quit()));
	win.connect(&pipe, SIGNAL(packet(QByteArray)), SLOT(dispatch(QByteArray)));

	pipe.start();
	return app.exec();
}

#include "eqml.moc"
