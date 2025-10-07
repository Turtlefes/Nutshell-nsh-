# Nutshell control file/profile

---

### .nshrc
You can use .nshrc as .bashrc as bash, you can put it in your HOME directory (or ~/.nshrc), example of .nshrc file contents:

```
PATH="/bin:/system/bin"
HISTFILE="$HOME/my_history"
echo Welcome users!, we are in the $$ PID!
```

#### Why .nshrc?
because you can set tasks when the shell starts, this is very useful, for example when you apply a variable when you exit it will be lost, but if you put it in an rc file it will be reinitialized when the shell starts

### .nshprofile
There is the user data profile, you can find files for bookmark paths, aliases, history, and even jobs (although jobs should not be touched), this is very useful because it is neatly structured there.