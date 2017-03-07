# AmplitudeHidingTest
This is a test for amplitude hiding project

## Istruction 

### For Decoding
In AmplitudeHidingTest
```
mv decode_frontend frontend
sudo ./configure
sudo make install
cd frontend 
sudo make
./lame --decode -f test.mp3 or ./lame --decode -f cello.mp3
mv  frontend decode_frontend
```

### ForEncoding
In AmplitudeHidingTest
```
mv encode_frontend frontend
sudo ./configure
sudo make install
cd frontend 
sudo make
./lame -s textToHiding -f test -e or ./lame -s textToHiding -f cello
mv  frontend encode_frontend
```
### File for test
*test.wav*
*cello.wav*
