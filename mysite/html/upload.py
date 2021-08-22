#!/usr/bin/env python3
# Import modules for CGI handling 
print ('Content-type:text/html\n')
print ()
#print ("Hello World")
#print("<br>")
#print(4+10)
import cgi, os
import cgitb; cgitb.enable()
form = cgi.FieldStorage()
# Get filename here.
print()
fileitem = form['filename']
# Test if the file was uploaded
if fileitem.filename:
   # strip leading path from file name to avoid
   # directory traversal attacks
   fn = os.path.basename(fileitem.filename.replace("\\", "/" ))
   open('/var/www/html/database/' + fn, 'wb').write(fileitem.file.read())
   message = 'The file "' + fn + '" was uploaded successfully'
else:
   message = 'No file was uploaded'
#print ("<html><body><p>%s</p></body></html>" % (message))


with open("/var/www/html/scriptsfile/successpage.html", "r", encoding='utf-8') as f:
    text=f.read()
    print(text)
'''for x in os.listdir():
    if x.endswith(".html"):
        # Prints only text file present in My Folder
        print(x)'''