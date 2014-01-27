#include <erl_interface.h>
#include <QThread>
#include <QtWidgets/QApplication>
#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>
#include <QtQml/QQmlEngine>

class eqmlTerm
{
	ETERM * m_term;
public:
	eqmlTerm(const eqmlTerm &);

	eqmlTerm(ETERM * term) : m_term(term)
	{
	}

	eqmlTerm(QByteArray & buffer)
	{
		m_term = erl_decode((unsigned char *)buffer.data());
	}

	~eqmlTerm()
	{
		erl_free_term(m_term);
	}

	eqmlTerm operator [] (int i) const
	{
		int size = erl_size(m_term);
		if (i <= 0 || i > size)
			qWarning("can't take %d element from tuple with arity %d", i, size);

		return eqmlTerm(erl_element(i, m_term));
	}

	QString atom() const
	{
		if (ERL_IS_ATOM(m_term))
			return QString(ERL_ATOM_PTR(m_term));

		qWarning("can't cast term to atom");
		return QString("undefined");
	}

	bool isInteger() const { return ERL_IS_INTEGER(m_term); }
	bool isFloat()   const { return ERL_IS_FLOAT(m_term);   }
	bool isAtom()    const { return ERL_IS_ATOM(m_term);    }
	bool isTuple()   const { return ERL_IS_TUPLE(m_term);   }
	bool isList()    const { return ERL_IS_LIST(m_term);    }

	int toInteger() const
	{ 
		return ERL_INT_VALUE(m_term);
	}

	float toFloat() const
	{ 
		return ERL_FLOAT_VALUE(m_term);
	}

	float toScalar() const
	{
		if (isInteger())
			return toInteger();
		if (isFloat())
			return toFloat();

		qWarning("can't cast term to scalar");
		return 0.0;
	}

	QString toString() const
	{
		char * s = erl_iolist_to_string(m_term);
		QString res(s);
		erl_free(s);
		return res;
	}

	QByteArray toArray() const
	{
		if (isAtom())
			return QByteArray(ERL_ATOM_PTR(m_term));

		char * s = erl_iolist_to_string(m_term);
		QByteArray res(s);
		erl_free(s);
		return res;
	}
};

class eqmlDepot
{
	typedef QVariant (eqmlDepot::*Handle)(const eqmlTerm &);
	typedef QMap<QByteArray, Handle> Registry;
	Registry registry;

	QVariant url(const eqmlTerm & t)
	{
		return QUrl(t.toString());
	}

	QVariant point(const eqmlTerm & t)
	{
		return QPointF(t[1].toScalar(), t[2].toScalar());
	}

public:
	eqmlDepot()
	{
		registry["url"] = &eqmlDepot::url;
		registry["point"] = &eqmlDepot::point;
	}

	QVariant var(const eqmlTerm & t)
	{
		if (t.isInteger())
			return t.toInteger();
		else if (t.isFloat())
			return t.toFloat();
		else if (t.isList())
			return t.toString();
		else if (t.isTuple())
		{
			QByteArray tag = t[1].toArray();

			Registry::iterator it = registry.find(tag);
			if (it != registry.end())
				return (this->*it.value())(t[2]);
			else
				qWarning("unknown tag \'%s\'", tag.data());
		}

		qWarning("can't cast term to QVariant");
		return QVariant();
	}
};
/*
class eqmlConnector
{
public:

};
*/
class eqmlLink : public QObject
{
	Q_OBJECT

	QDataStream & outStream;

	QByteArray tag;
	QByteArray pid;

	void fire(const QVariantList & l)
	{
		ETERM * terms[8];

		terms[0] = erl_mk_atom("signal");
		terms[1] = erl_mk_string(pid.data());
		terms[2] = erl_mk_atom(tag.data());

		for (int i = 0; i < l.size(); ++i)
		{
			const QVariant & v = l.at(i);
			switch (v.type())
			{
				case QMetaType::Int:
					terms[i + 3] = erl_mk_int(v.toInt());
					break;
				case QMetaType::Double:
					terms[i + 3] = erl_mk_float(v.toFloat());
					break;
				case QMetaType::QString:
					terms[i + 3] = erl_mk_string(qPrintable(v.toString()));
					break;
				default:
					qWarning("can't cast QVariant to term");
			}
		}

		ETERM * tuple = erl_mk_tuple(terms, l.size() + 3);

		int len = erl_term_len(tuple);
		char * buf = new char[len+1];
		erl_encode(tuple, (unsigned char *)buf);

		outStream << len;
		outStream.writeRawData(buf, len);

		delete[] buf;
		erl_free_compound(tuple);
	}

public:
	eqmlLink(QDataStream & _outStream, const QByteArray & _tag, const QByteArray & _pid)
		: outStream(_outStream), tag(_tag), pid(_pid)
	{
	}

public slots:
	void link()
	{
		QVariantList l;
		fire(l);
	}

	void link(const QVariant & a)
	{
		QVariantList l;
		l.push_back(a);
		fire(l);
	}

	void link(const QVariant & a, const QVariant & b)
	{
		QVariantList l;
		l.push_back(a);	l.push_back(b);
		//fin(push(push(prep(),a),b))
		fire(l);
	}

	void link(const QVariant & a, const QVariant & b, const QVariant & c)
	{
		QVariantList l;
		l.push_back(a);	l.push_back(b);	l.push_back(c);
		fire(l);
	}

	void link(const QVariant & a, const QVariant & b, const QVariant & c, const QVariant & d)
	{
		QVariantList l;
		l.push_back(a);	l.push_back(b);	l.push_back(c);	l.push_back(d);
		fire(l);
	}

	void link(const QVariant & a, const QVariant & b, const QVariant & c, const QVariant & d, const QVariant & e)
	{
		QVariantList l;
		l.push_back(a);	l.push_back(b);	l.push_back(c);	l.push_back(d); l.push_back(e);
		fire(l);
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

	eqmlDepot depot;

	QFile outFile;
	QDataStream outStream;

	QObject * find(const QByteArray & a)
	{
		QObject * obj = rootObject()->findChild<QObject *>(a);
		if (obj == NULL && a == "root")
			obj = rootObject();
		if (!obj)
			qWarning("can't find child %s", a.data());
		return obj;
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
		registry["invoke"] = &eqmlWindow::onInvoke;
		registry["set"] = &eqmlWindow::onSet;
	}

	void onConnect(const eqmlTerm & term)
	{
		QObject * obj = find(term[1].toArray());
		if (!obj)
			return;

		QByteArray signalName = term[2].toArray();
		QByteArray tag = term[3].toArray();
		int order = term[4].toInteger();
		QByteArray pid = term[5].toArray();

		QByteArray Args = "(";
		for (int i = 0; i < order; i++)
		{			
			Args.append("QVariant");
			if (i < order - 1)
				Args.append(",");
		}
		Args.append(")");

		int signalIdx = obj->metaObject()->indexOfSignal(signalName.append(Args));
		QMetaMethod signalMethod = obj->metaObject()->method(signalIdx);

		eqmlLink * link = new eqmlLink(outStream, tag, pid);

		int slotIdx = link->metaObject()->indexOfSlot(Args.prepend("link"));
		QMetaMethod slotMethod = link->metaObject()->method(slotIdx);

		if (!QObject::connect(obj, signalMethod, link, slotMethod))
			qWarning("connection fail");
	}	

	void onInvoke(const eqmlTerm & term)
	{
		QObject * obj = find(term[1].toArray());
		if (obj)
			QMetaObject::invokeMethod(obj, term[2].toArray());
	}	

	void onSet(const eqmlTerm & term)
	{
		QObject * obj = find(term[1].toArray());
		if (obj)
			obj->setProperty(term[2].toArray(),	depot.var(term[3]));
	}

public slots:
	void dispatch(QByteArray buffer)
	{
		eqmlTerm term(buffer);
		QByteArray tag = term[1].toArray();

		Registry::iterator it = registry.find(tag);
		if (it != registry.end())
			(this->*it.value())(term[2]);
		else
			qWarning("unknown tag \'%s\'", tag.data());
	}
};

void eqmlLog(QtMsgType type, const QMessageLogContext & ctx, const QString & msg)
{
	type = type;
	fprintf(stderr, "%s:%u: %s\r\n", ctx.file, ctx.line, qPrintable(msg));
}

int main(int argc, char *argv[])
{
	qInstallMessageHandler(eqmlLog);
    QApplication app(argc, argv);
	erl_init(NULL, 0);

	eqmlPipe pipe;
	eqmlWindow win(app.arguments().at(1));

	app.connect(&pipe, SIGNAL(finished()), SLOT(quit()));
	win.connect(&pipe, SIGNAL(packet(QByteArray)), SLOT(dispatch(QByteArray)));

	pipe.start();
    return app.exec();
}

#include "eqml.moc"
