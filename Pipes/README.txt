Salut,
Aici sunt 3 fisiere sursa:

pipe-anon.c  -- este fisierul sursa ce rezolva o problema de achizitie de date de la 
				senzori folosin mai multi fii reprezentativi pentru fiecare senzor in parte.
				Schema de stari si de comunicare este imagine structuraComunicare.png
				Aceasta inplementare a fost testata si lasata ca functionala.

pipe-nume-wr.c -- este implementarea aceleasi solutii folosind pipuri cu nume, structura de baza se pastreaza.
				  Aici am dat de problema ca se blocheaza procesul fara a primi vre-o eroare la momentul deschiderii
				  pipeurilor cu nume.
				  
unu.c -- Descrie aceasi problema ca mai sus intr-un cadru simplificat, alaturi de imaginea dovada 
		 SOTR-BlocareDeschidereFisier.png
		 
