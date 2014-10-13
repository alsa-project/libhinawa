from gi.repository import GHinawa

hoge=10

def pero(obj, data1, data2):
	print(type(obj))
	print(data1)
	print(data2)

hoge = GHinawa.SndUnit()

fuga=100

hoge.set_cb(pero, fuga)
