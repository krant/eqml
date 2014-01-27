all: port eqml.beam

eqml.beam: eqml.erl
	@erlc eqml.erl

port: eqml.make
	@make -s -f eqml.make

eqml.make: eqml.pro
	@qmake eqml.pro -o eqml.make

eqml.pro:
	@echo "TARGET = eqml" > eqml.pro
	@echo "QT += qml quick widgets" >> eqml.pro
	@echo "SOURCES += eqml.cpp" >> eqml.pro
	@echo "INCLUDEPATH += `erl -noshell -eval "io:format(code:lib_dir(erl_interface, include)),erlang:halt(0)."`">> eqml.pro
	@echo "LIBS += -L`erl -noshell -eval "io:format(code:lib_dir(erl_interface, lib)),erlang:halt(0)."`" >> eqml.pro
	@echo "LIBS += -lerl_interface_st -lei_st" >> eqml.pro

clean:
	@rm -f eqml eqml.beam eqml.pro eqml.make eqml.o eqml.moc
