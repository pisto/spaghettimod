
/*
 * DTrace Sauerbraten provider
 */
/*
#pragma D attributes Evolving/Evolving/Common provider myserv provider
#pragma D attributes Evolving/Evolving/Common provider myserv module
#pragma D attributes Evolving/Evolving/Common provider myserv function
#pragma D attributes Evolving/Evolving/Common provider myserv name
#pragma D attributes Evolving/Evolving/Common provider myserv args
*/

provider sauerbraten {
	probe command__entry(char *, char *, char *, char *);
	probe command__return(char *);
	probe var__entry(char *, char *);
	probe var__return(char *, int);
	probe alias__entry(char *, char *, char *, char *);
	probe alias__return(char *);
};

