import calcimp

f,re,im,mag = calcimp.calcimp("../sample/test.men")

for fi, rei, imi,magi in zip(f,re,im,mag):
	print(f"{fi},{rei},{imi},{magi}")

exit()


