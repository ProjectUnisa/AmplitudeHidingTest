# AmplitudeHidingTest
This is a test for amplitude hiding project

## Istruction 

### For Decoding
In AmplitudeHidingTest
```
mv encode_frontend frontend
sudo ./configure
sudo make install
cd frontend 
sudo make
./lame --decode -f test.mp3 or ./lame --decode -f cello.mp3
```

### ForEncoding
In AmplitudeHidingTest
```
mv frontend decode_frontend
sudo ./configure
sudo make install
cd frontend 
sudo make
./lame -s textToHiding -f test -e or ./lame -s textToHiding -f cello
```
